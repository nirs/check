package main

import (
	"sync"
	"syscall"
	"time"
)

type Checker struct {
	path   string
	ticker *time.Ticker
	stop   chan bool
}

func newChecker(path string, interval int) *Checker {
	return &Checker{
		path:   path,
		ticker: time.NewTicker(time.Second * time.Duration(interval)),
		stop:   make(chan bool, 1),
	}
}

func (ck *Checker) Start() {
	go ck.run()
}

func (ck *Checker) Stop() {
	// TODO: send EINPROGRESS event if the checker is busy
	ck.stop <- true
}

func (ck *Checker) run() {
	sendEvent("start", ck.path, 0, "started")
	ck.check()

loop:
	for {
		select {
		case <-ck.ticker.C:
			ck.check()
		case <-ck.stop:
			break loop
		}
	}

	ck.ticker.Stop()

	checkersMutex.Lock()
	delete(checkers, ck.path)
	checkersMutex.Unlock()

	sendEvent("stop", ck.path, 0, "stopped")
}

func (ck *Checker) check() {
	// TODO: mark checker as busy during check
	time.Sleep(time.Microsecond * 500)
	sendEvent("check", ck.path, 0, "0.0005")
}

var (
	checkers      = make(map[string]*Checker)
	checkersMutex = sync.Mutex{}
)

func startChecking(path string, interval int) {
	checkersMutex.Lock()
	defer checkersMutex.Unlock()
	ck, ok := checkers[path]
	if ok {
		sendEvent("start", path, syscall.EEXIST, "already checking path")
		return
	}
	ck = newChecker(path, interval)
	checkers[path] = ck
	ck.Start()
}

func stopChecking(path string) {
	checkersMutex.Lock()
	defer checkersMutex.Unlock()
	ck, ok := checkers[path]
	if !ok {
		sendEvent("stop", path, syscall.ENOENT, "not checking path")
		return
	}
	ck.Stop()
}

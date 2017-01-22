package main

import (
	"fmt"
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
	// Signal checker without blocking
	select {
	case ck.stop <- true:
		logDebug("signaled checker %q", ck.path)
	default:
		logDebug("checker %q already signaled", ck.path)
	}
}

func (ck *Checker) run() {
	logInfo("checker %q started", ck.path)
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
	logInfo("checker %q stopped", ck.path)
}

func (ck *Checker) check() {
	logDebug("starting check path %q...", ck.path)
	start := time.Now()

	time.Sleep(time.Microsecond * 500)

	elapsed := time.Since(start).Seconds()
	logDebug("checking path %q completed in %f seconds", ck.path, elapsed)

	sendEvent("check", ck.path, 0, fmt.Sprintf("%f", elapsed))
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
		logWarn("already checking path %s", path)
		sendEvent("start", path, syscall.EEXIST, "already checking path")
		return
	}
	logInfo("start checking path %q every %d seconds", path, interval)
	ck = newChecker(path, interval)
	checkers[path] = ck
	ck.Start()
}

func stopChecking(path string) {
	checkersMutex.Lock()
	defer checkersMutex.Unlock()
	ck, ok := checkers[path]
	if !ok {
		logWarn("not checking path %s", path)
		sendEvent("stop", path, syscall.ENOENT, "not checking path")
		return
	}
	logInfo("stop checking path %q", path)
	ck.Stop()
}

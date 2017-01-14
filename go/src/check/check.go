package main

import (
	"syscall"
	"time"
)

type Checker struct {
}

var (
	checkers = map[string]Checker{}
)

func startChecking(path string, interval int) {
	_, ok := checkers[path]
	if ok {
		sendEvent("start", path, syscall.EEXIST, "already checking path")
		return
	}
	checkers[path] = Checker{}
	sendEvent("start", path, 0, "started")

	// Fake the first check
	go func() {
		time.Sleep(time.Microsecond * 500)
		sendEvent("check", path, 0, "0.0005")
	}()
}

func stopChecking(path string) {
	_, ok := checkers[path]
	if !ok {
		sendEvent("stop", path, syscall.ENOENT, "not checking path")
		return
	}

	// Fake checker that can be stopped immediately.
	delete(checkers, path)
	sendEvent("stop", path, 0, "stopped")
}

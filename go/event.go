package main

import (
	"fmt"
	"os"
	"syscall"
	"time"
)

const (
	sendTimeout = 2 * time.Second
	queueSize   = 128
)

var (
	queue = make(chan string, queueSize)
)

func init() {
	go processEvents()
}

// Send an event to the parrent process
func sendEvent(name string, path string, errno syscall.Errno, data string) {
	event := fmt.Sprintf("%s %s %d %s\n", name, path, errno, data)

	select {
	case queue <- event:
		logDebug("sent event %q", event)
	case <-time.After(sendTimeout):
		logError("timeout sending event, terminating")
		os.Exit(1)
	}
}

func processEvents() {
	for event := range queue {
		logDebug("writing event %q", event)
		// This can block without limit if parrent is not reading, we handle
		// this in sendEvent.
		_, err := os.Stdout.WriteString(event)
		if err != nil {
			logError("cannot write to stdout, terminating: %s", err)
			os.Exit(1)
		}
	}
}

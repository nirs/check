package main

import (
	"fmt"
	"os"
	"syscall"
)

// Send an event to the parrent process
func sendEvent(name string, path string, errno syscall.Errno, data string) {
	event := fmt.Sprintf("%s %s %d %s\n", name, path, errno, data)

	// TODO: add logger
	fmt.Fprintf(os.Stderr, "DEBUG sending event %q\n", event)

	// TODO: this can block if parrent is not reading, and chaning os.Stdout to
	// non-blocking does not work.
	_, err := os.Stdout.WriteString(event)
	if err != nil {
		fmt.Fprintf(os.Stderr, "cannot write to stdout, terminating: %s\n", err)
		os.Exit(1)
	}
}

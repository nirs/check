package main

import (
	"bufio"
	"os"
	"strconv"
	"strings"
	"syscall"
)

const (
	maxCommandArgs   = 3
	minCheckInterval = 1
	maxCheckInterval = 3600
)

func main() {
	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		line := scanner.Text()
		args := strings.SplitN(line, " ", maxCommandArgs)
		cmd := args[0]
		switch cmd {
		case "":
			sendEvent("-", "-", syscall.EINVAL, "empty command")
		case "start":
			if len(args) < 2 {
				sendEvent(cmd, "-", syscall.EINVAL, "path is required")
				continue
			}
			path := args[1]
			if len(args) < 3 {
				sendEvent(cmd, path, syscall.EINVAL, "interval is required")
				continue
			}
			interval, err := strconv.Atoi(args[2])
			if err != nil {
				sendEvent(cmd, path, syscall.EINVAL, "invalid interval")
				continue
			}
			if interval < minCheckInterval || interval > maxCheckInterval {
				sendEvent(cmd, path, syscall.EINVAL, "interval out of range")
				continue
			}
			sendEvent(cmd, path, 0, "started")
			sendEvent("check", path, 0, "0.0005")
		case "stop":
			if len(args) < 2 {
				sendEvent(cmd, "-", syscall.EINVAL, "path is required")
				continue
			}
			path := args[1]
			sendEvent(cmd, path, 0, "stopped")
		default:
			sendEvent(cmd, "-", syscall.EINVAL, "unknown command")
		}
	}
}

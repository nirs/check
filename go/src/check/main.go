package main

import (
	"bufio"
	"flag"
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

var (
	debugMode = flag.Bool("debug", false, "Enable debug mode (false)")
)

func main() {
	flag.Parse()
	if *debugMode {
		setLogLevel(levelDebug)
	}
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
			startChecking(path, interval)
		case "stop":
			if len(args) < 2 {
				sendEvent(cmd, "-", syscall.EINVAL, "path is required")
				continue
			}
			path := args[1]
			stopChecking(path)
		default:
			sendEvent(cmd, "-", syscall.EINVAL, "unknown command")
		}
	}
}

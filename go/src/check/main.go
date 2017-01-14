package main

import (
	"bufio"
	"fmt"
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
			eventPrint("-", "-", syscall.EINVAL, "empty commnad")
		case "start":
			if len(args) < 2 {
				eventPrint(cmd, "-", syscall.EINVAL, "path is required")
				continue
			}
			path := args[1]
			if len(args) < 3 {
				eventPrint(cmd, path, syscall.EINVAL, "interval is required")
				continue
			}
			interval, err := strconv.Atoi(args[2])
			if err != nil {
				eventPrint(cmd, path, syscall.EINVAL, "invalid internval")
				continue
			}
			if interval < minCheckInterval || interval > maxCheckInterval {
				eventPrint(cmd, path, syscall.EINVAL, "interval out of range")
				continue
			}
			eventPrint(cmd, path, 0, "started")
			eventPrint("check", path, 0, "0.0005")
		case "stop":
			if len(args) < 2 {
				eventPrint(cmd, "-", syscall.EINVAL, "path is required")
				continue
			}
			path := args[1]
			eventPrint(cmd, path, 0, "stopped")
		default:
			eventPrint(cmd, "-", syscall.EINVAL, "unknown command")
		}
	}
}

func eventPrint(name string, path string, errno syscall.Errno, msg string) {
	fmt.Fprintf(os.Stdout, "%s %s %d %s\n", name, path, errno, msg)
}

package main

import (
	"bufio"
	"flag"
	"os"
	"strconv"
	"strings"
	"syscall"

	"ovirt.org/check/directio"
	"ovirt.org/check/event"
	"ovirt.org/check/monitor"
	"ovirt.org/check/log"
)

const (
	maxCommandArgs   = 3
	minCheckInterval = 1
	maxCheckInterval = 3600
	checkSize        = 4096
)

var (
	debugMode bool
)

func init() {
	flag.BoolVar(&debugMode, "debug", false, "Enable debug mode")
}

func main() {
	flag.Parse()
	if debugMode {
		log.SetLevel(log.DEBUG)
	}
	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		line := scanner.Text()
		args := strings.SplitN(line, " ", maxCommandArgs)
		cmd := args[0]
		switch cmd {
		case "start":
			if len(args) < 2 {
				event.Send(cmd, "-", syscall.EINVAL, "path is required")
				continue
			}
			path := args[1]
			if len(args) < 3 {
				event.Send(cmd, path, syscall.EINVAL, "interval is required")
				continue
			}
			interval, err := strconv.Atoi(args[2])
			if err != nil {
				event.Send(cmd, path, syscall.EINVAL, "invalid interval")
				continue
			}
			if interval < minCheckInterval || interval > maxCheckInterval {
				event.Send(cmd, path, syscall.EINVAL, "interval out of range")
				continue
			}
			checker := directio.NewChecker(checkSize)
			monitor.Start(path, interval, checker)
		case "stop":
			if len(args) < 2 {
				event.Send(cmd, "-", syscall.EINVAL, "path is required")
				continue
			}
			path := args[1]
			monitor.Stop(path)
		default:
			log.Warn("received unknown command %q", args)
			if cmd == "" {
				cmd = "-"
			}
			event.Send(cmd, "-", syscall.EINVAL, "unknown command")
		}
	}
}

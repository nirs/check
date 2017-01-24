package main

import (
	"fmt"
	"os"
)

const (
	levelDebug = 0
	levelInfo  = 1
	levelWarn  = 2
	levelError = 3
)

var (
	levelName    = []string{"DEBUG", "INFO", "WARN", "ERROR"}
	currentLevel = levelInfo
)

func setLogLevel(newLevel int) {
	currentLevel = newLevel
}

func logDebug(format string, args ...interface{}) {
	logLevel(levelDebug, format, args...)
}

func logInfo(format string, args ...interface{}) {
	logLevel(levelInfo, format, args...)
}

func logWarn(format string, args ...interface{}) {
	logLevel(levelWarn, format, args...)
}

func logError(format string, args ...interface{}) {
	logLevel(levelError, format, args...)
}

func logLevel(level int, format string, args ...interface{}) {
	if level >= currentLevel {
		fmt.Fprintf(os.Stderr, levelName[level]+" "+format+"\n", args...)
	}
}

package log

import (
	"fmt"
	"os"
)

type Level uint

const (
	DEBUG Level = 0 + iota
	INFO
	WARN
	ERROR
)

var (
	currentLevel = INFO
)

func (n Level) String() string {
	switch n {
	case DEBUG:
		return "DEBUG"
	case INFO:
		return "INFO"
	case WARN:
		return "WARN"
	default:
		return "ERROR"
	}
}

func SetLevel(n Level) {
	currentLevel = n
}

func Debug(format string, args ...interface{}) {
	log(DEBUG, format, args...)
}

func Info(format string, args ...interface{}) {
	log(INFO, format, args...)
}

func Warn(format string, args ...interface{}) {
	log(WARN, format, args...)
}

func Error(format string, args ...interface{}) {
	log(ERROR, format, args...)
}

func log(n Level, format string, args ...interface{}) {
	if n >= currentLevel {
		fmt.Fprintf(os.Stderr, n.String()+" "+format+"\n", args...)
	}
}

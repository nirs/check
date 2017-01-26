package directio

import (
	"os"
	"syscall"
	"time"
	"unsafe"
)

const (
	bufAlign = 512
)

type Checker struct {
	buf []byte
}

func NewChecker(size int) *Checker {
	return &Checker{alignedBuffer(size, bufAlign)}
}

func (c *Checker) ReadDelay(path string) (float64, syscall.Errno) {
	start := time.Now()

	flags := os.O_RDONLY | syscall.O_DIRECT | syscall.O_CLOEXEC
	file, err := os.OpenFile(path, flags, 0)
	if err != nil {
		return 0, errorNumber(err)
	}

	defer file.Close()

	_, err = file.Read(c.buf)
	if err != nil {
		return 0, errorNumber(err)
	}

	return time.Since(start).Seconds(), 0
}

func alignedBuffer(size int, align int) []byte {
	buf := make([]byte, size+align)
	offset := 0
	remainder := int(uintptr(unsafe.Pointer(&buf[0])) & uintptr(align-1))
	if remainder != 0 {
		offset = align - remainder
	}
	return buf[offset : offset+size]
}

// errorNumber extract syscall.Errno from os errors.
func errorNumber(e error) syscall.Errno {
	switch e := e.(type) {
	case *os.PathError:
		return errorNumber(e.Err)
	case syscall.Errno:
		return e
	default:
		return syscall.EIO
	}
}

package main

import (
	"os"
	"syscall"
	"time"
	"unsafe"
)

func readDelay(path string) (float64, syscall.Errno) {
	start := time.Now()

	flags := os.O_RDONLY | syscall.O_DIRECT | syscall.O_CLOEXEC
	file, err := os.OpenFile(path, flags, 0)
	if err != nil {
		return 0, errorNumber(err)
	}

	defer file.Close()

	buf := alignedBuffer(BLOCK_SIZE)
	_, err = file.Read(buf)
	if err != nil {
		return 0, errorNumber(err)
	}

	return time.Since(start).Seconds(), 0
}

func alignedBuffer(size int) []byte {
	buf := make([]byte, size+BLOCK_ALIGN)
	offset := 0
	remainder := int(uintptr(unsafe.Pointer(&buf[0])) & uintptr(BLOCK_ALIGN-1))
	if remainder != 0 {
		offset = BLOCK_ALIGN - remainder
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

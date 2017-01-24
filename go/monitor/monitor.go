package monitor

import (
	"fmt"
	"sync"
	"syscall"
	"time"

	event "check/go/event"
	log "check/go/log"
)

var (
	monitors = make(map[string]*Monitor)
	mutex    = sync.Mutex{}
)

// Start monitoring path
func Start(path string, interval int) {
	mutex.Lock()
	defer mutex.Unlock()
	m, ok := monitors[path]
	if ok {
		log.Warn("already checking path %s", path)
		event.Send("start", path, syscall.EEXIST, "already checking path")
		return
	}
	log.Info("start checking path %q every %d seconds", path, interval)
	m = newMonitor(path, interval)
	monitors[path] = m
	m.Start()
}

// Stop monitoring path
func Stop(path string) {
	mutex.Lock()
	defer mutex.Unlock()
	m, ok := monitors[path]
	if !ok {
		log.Warn("not checking path %s", path)
		event.Send("stop", path, syscall.ENOENT, "not checking path")
		return
	}
	log.Info("stop checking path %q", path)
	m.Stop()
}

func monitorStopped(m *Monitor) {
	mutex.Lock()
	delete(monitors, m.path)
	mutex.Unlock()
}

type Monitor struct {
	path   string
	ticker *time.Ticker
	stop   chan bool
}

func newMonitor(path string, interval int) *Monitor {
	return &Monitor{
		path:   path,
		ticker: time.NewTicker(time.Second * time.Duration(interval)),
		stop:   make(chan bool, 1),
	}
}

func (m *Monitor) Start() {
	go m.run()
}

func (m *Monitor) Stop() {
	// Signal monitor without blocking
	select {
	case m.stop <- true:
		log.Debug("signaled monitor %q", m.path)
	default:
		log.Debug("monitor %q already signaled", m.path)
	}
}

func (m *Monitor) run() {
	log.Info("monitor %q started", m.path)
	event.Send("start", m.path, 0, "started")
	m.check()

	for {
		select {
		case <-m.ticker.C:
			m.check()
		case <-m.stop:
			m.ticker.Stop()
			monitorStopped(m)
			event.Send("stop", m.path, 0, "stopped")
			log.Info("monitor %q stopped", m.path)
			return
		}
	}
}

func (m *Monitor) check() {
	log.Debug("checking %q...", m.path)

	delay, err := readDelay(m.path)
	if err != 0 {
		log.Debug("check %q failed: %s", m.path, err)
		event.Send("check", m.path, err, err.Error())
		return
	}

	log.Debug("check %q completed in %f seconds", m.path, delay)
	event.Send("check", m.path, 0, fmt.Sprintf("%f", delay))
}

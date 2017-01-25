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
		log.Warn("already monitoring path %s", path)
		event.Send("start", path, syscall.EEXIST, "already monitoring path")
		return
	}
	log.Info("start monitoring path %q (interval=%d)", path, interval)
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
		log.Warn("not monitoring path %s", path)
		event.Send("stop", path, syscall.ENOENT, "not monitoring path")
		return
	}
	log.Info("stop monitoring path %q", path)
	m.Stop()
}

func monitorStopped(m *Monitor) {
	mutex.Lock()
	delete(monitors, m.path)
	mutex.Unlock()
}

type monitorState int

const (
	WAITING monitorState = 0 + iota
	CHECKING
	STOPPING
)

type Monitor struct {
	path     string
	state    monitorState
	ticker   *time.Ticker
	stop     chan bool
	complete chan bool
	start    time.Time
}

func newMonitor(path string, interval int) *Monitor {
	return &Monitor{
		path:     path,
		state:    WAITING,
		ticker:   time.NewTicker(time.Second * time.Duration(interval)),
		stop:     make(chan bool),
		complete: make(chan bool),
	}
}

func (m *Monitor) Start() {
	go m.run()
}

func (m *Monitor) Stop() {
	m.stop <- true
}

func (m *Monitor) run() {
	log.Info("monitor %q started", m.path)
	event.Send("start", m.path, 0, "started")

	m.check()

loop:
	for {
		select {
		case <-m.ticker.C:
			switch m.state {
			case WAITING:
				m.check()
			default:
				log.Warn("monitor %q is blocked for %f seconds",
					m.path, time.Since(m.start).Seconds())
			}
		case <-m.complete:
			switch m.state {
			case CHECKING:
				m.state = WAITING
			case STOPPING:
				// Was stopped during check
				break loop
			}
		case <-m.stop:
			switch m.state {
			case WAITING:
				break loop
			case CHECKING:
				m.state = STOPPING
				log.Debug("monitor %q is checking, waiting until io completes", m.path)
				event.Send("stop", m.path, syscall.EINPROGRESS, "stopping in progress")
			case STOPPING:
				log.Debug("monitor %q stopping in progress", m.path)
				event.Send("stop", m.path, syscall.EINPROGRESS, "stopping in progress")
			}
		}
	}

	m.ticker.Stop()
	monitorStopped(m)
	event.Send("stop", m.path, 0, "stopped")
	log.Info("monitor %q stopped", m.path)
}

func (m *Monitor) check() {
	log.Debug("checking %q...", m.path)

	m.state = CHECKING
	m.start = time.Now()

	go func() {
		defer func() { m.complete <- true }()

		delay, err := readDelay(m.path)
		if err != 0 {
			log.Error("check %q failed: %s", m.path, err)
			event.Send("check", m.path, err, err.Error())
			return
		}

		log.Debug("check %q completed in %f seconds", m.path, delay)
		event.Send("check", m.path, 0, fmt.Sprintf("%f", delay))
	}()
}

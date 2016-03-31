# check - path checking helper

This program is providing path checking services for vdsm.

The program is running as a child process, accepting commands on stdin,
sending events on stdout, and logging messages on stderr.

## Commands

- start PATH INTERVAL - start checking PATH every INTERVAL seconds

- stop PATH - stop checking PATH

## Events

- started PATH - path checker for path has started. Sent before the
  first check was completed. If path is accessible, will be followed
  soon by "completed" event.

- completed PATH DELAY ERROR - checking PATH completed in DELAY seconds with
  error ERRNO. Non-zero ERRNO means path is not accessible.

- stopped PATH - path checker for path has stopped. No more events will
  be generated for PATH.

- error PATH ERRNO - "start" or "stop" failed with error ERRNO

## Logging

- LEVEL message... - log message at LEVEL

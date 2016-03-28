check - path checking helper

This program is providing path checking services for vdsm.

The program is running as a child process, accepting commands on stdin,
sending events on stdout, and logging messages on stderr.

Commands:

  start PATH INTERVAL       start checking PATH every INTERVAL seconds

  stop PATH                 stop checking PATH

Events:

  check PATH DELAY          check PATH completed in DELAY seconds

  error PATH TIME CODE      checking patch starting at TIME failed with
                            error code
Logging:

  LEVEL message...          log message at LEVEL

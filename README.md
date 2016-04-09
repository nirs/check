# check - path checking helper

This program is providing path checking services for vdsm.

The program is running as a child process, accepting commands on stdin,
sending events on stdout, and logging messages on stderr.

## Commands

- start PATH INTERVAL

  Start checking PATH every INTERVAL seconds. A "start" event will be
  sent as response.

- stop PATH

  Stop checking PATH. A "stop" event will be sent when the
  path checker has stopped.

## Events

Events always have 4 fields. Missing values are represented as "-".

  NAME PATH ERRNO DATA

Available events:

- start PATH ERRNO -

  A "start" command completed. On success (ERRNO=0), a "check" event
  will be sent when the first check completes.

- check PATH ERRNO [DELAY|-]

  A check for PATH completed. On success (ERRNO=0), DELAY is the read
  delay in seconds.

- stop PATH ERRNO -

  A "stop" command completed. On success (ERRNO=0), no more "check"
  events will be sent. If path checker is busy and cannot be stopped
  immediately , an EINPROGRESS error is returned, and a second event
  will be sent when io has completed and the path checker has stopped.

## Logging

- LEVEL message... - log message at LEVEL

The parent process may log the messages using its own logger.

## Example session

```
time   sender  message
---------------------------------------------------
00.000 Client: start /dev/vgname/lvname 10
00.001 Server: start /dev/vgname/lvname 0 -
00.002 Server: check /dev/vgname/lvname 0 0.000542
10.002 Server: check /dev/vgname/lvname 0 0.001023
20.001 Server: check /dev/vgname/lvname 0 0.000981
35.345 Server: check /dev/vgname/lvname 5 -
40.001 Server: check /dev/vgname/lvname 5 -
50.001 Server: check /dev/vgname/lvname 5 -
61.655 Server: check /dev/vgname/lvname 0 1.654321
69.000 Client: stop /dev/vgname/lvname
69.001 Server: stop /dev/vgname/lvname 0 -
```

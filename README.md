# check - path checking helper

This program is providing path checking services for vdsm.

The program is running as a child process, accepting commands on stdin,
sending events on stdout, and logging messages on stderr.

## Commands

- `start PATH INTERVAL`

  Start checking PATH every INTERVAL seconds. A "start" event will be
  sent as response.

- `stop PATH`

  Stop checking PATH. A "stop" event will be sent as a response.

## Events

- `start PATH ERRNO DESCRIPTION`

  A "start" command completed. On success (`ERRNO=0`), a "check" event
  will be sent when the first check completes.

- `check PATH ERRNO [DELAY|DESCRIPTION]`

  A check for PATH completed. On success (`ERRNO=0`), DELAY is the read
  delay in seconds.

- `stop PATH ERRNO DESCRIPTION`

  A "stop" command completed. On success (`ERRNO=0`), no more "check"
  events will be sent. If path checker is busy and cannot be stopped
  immediately , an EINPROGRESS error is returned, and a second event
  will be sent when io has completed and the path checker has stopped.

## Logging

- `LEVEL message...` - log message at LEVEL

The parent process may log the messages using its own logger.

## Example session

```
time   sender  message
---------------------------------------------------
00.000 Client: start /path 10
00.001 Server: start /path 0 started
00.002 Server: check /path 0 0.000542
10.002 Server: check /path 0 0.001023
20.001 Server: check /path 0 0.000981
35.345 Server: check /path 5 Input/output error
40.001 Server: check /path 5 Input/output error
50.001 Server: check /path 5 Input/output error
61.655 Server: check /path 0 1.654321
69.000 Client: stop /path
69.001 Server: stop /path 0 stopped
```

## Hacking

Clone the source from gitlab:

```
git clone https://gitlab.com/nirs/check.git
```

Required packages - Fedora
```
dnf install golang make
```

Required packages - Debian
```
apt-get install golang-go make
```

Required packages - common
```
pip install -r requirements.txt
```

Building:
```
make
```

Running the tests:
```
make test
```

Please open a merge request in gitlab.

## Containers

To build the containers locally:

    cd containers
    make

To run the tests in a container:

    podman run \
        -it \
        --rm \
        --volume `pwd`:/src:Z \
        check-fedora-35 \
        bash -c "cd src && make test"

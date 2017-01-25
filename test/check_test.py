# Copyright 2016 Nir Soffer <nsoffer@redhat.com>
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v2 or (at your option) any later version.

from __future__ import print_function
import errno
import subprocess
import time
from collections import namedtuple

import pytest


Event = namedtuple("Event", "name,path,error,data")


class Checker(object):

    def __init__(self, path):
        self.proc = subprocess.Popen([path],
                                     stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE)

    def send(self, *args):
        msg = " ".join(args) + "\n"
        self.proc.stdin.write(msg)
        self.proc.stdin.flush()

    def recv(self):
        event = self.proc.stdout.readline().strip()
        args = event.split(None, 3)
        return Event(*args)

    def close(self):
        self.proc.terminate()
        self.proc.wait()


@pytest.yield_fixture(params=["./c/check", "./go/check"])
def checker(request):
    c = Checker(request.param)
    yield c
    c.close()


def test_start_file(tmpdir, checker):
    path = tmpdir.join("file")
    path.write("x")
    checker.send("start", str(path), "1")
    event = checker.recv()
    assert_success(event, "start", str(path))
    event = checker.recv()
    assert_success(event, "check", str(path))
    assert float(event.data) > 0.0


def test_stop_file(tmpdir, checker):
    path = tmpdir.join("file")
    path.write("x")
    checker.send("start", str(path), "1")
    checker.recv()  # started
    checker.recv()  # first check
    checker.send("stop", str(path))
    event = checker.recv()
    assert_success(event, "stop", str(path))


def test_start_missing_file(checker):
    path = "nosuchfile"
    checker.send("start", path, "1")
    event = checker.recv()
    assert_success(event, "start", path)
    event = checker.recv()
    assert_error(event, "check", path, errno.ENOENT)


def test_check_file_appear(tmpdir, checker):
    path = tmpdir.join("file")
    checker.send("start", str(path), "1")
    checker.recv()  # started
    checker.recv()  # first check
    path.write("x")
    event = checker.recv()
    assert_success(event, "check", str(path))
    assert float(event.data) > 0.0


def test_check_file_disapear(tmpdir, checker):
    path = tmpdir.join("file")
    path.write("x")
    checker.send("start", str(path), "1")
    checker.recv()  # started
    checker.recv()  # first check
    path.remove()
    event = checker.recv()
    assert_error(event, "check", str(path), errno.ENOENT)


def test_start_already_checking(tmpdir, checker):
    path = tmpdir.join("file")
    path.write("x")
    checker.send("start", str(path), "1")
    checker.recv()  # started
    checker.recv()  # first check
    checker.send("start", str(path), "1")
    event = checker.recv()
    assert_error(event, "start", str(path), errno.EEXIST)


def test_stop_unchecked_path(tmpdir, checker):
    checker.send("stop", "path")
    event = checker.recv()
    assert_error(event, "stop", "path", errno.ENOENT)


def test_start_no_path(checker):
    checker.send("start")
    event = checker.recv()
    assert_error(event, "start", "-", errno.EINVAL)


def test_start_no_interval(checker):
    checker.send("start", "path")
    event = checker.recv()
    assert_error(event, "start", "path", errno.EINVAL)


def test_stop_no_path(checker):
    checker.send("stop")
    event = checker.recv()
    assert_error(event, "stop", "-", errno.EINVAL)


def test_uknown_cmd(checker):
    checker.send("unknown", "path", "1")
    event = checker.recv()
    assert_error(event, "unknown", "-", errno.EINVAL)


def test_empty_cmd(checker):
    checker.send("", "path", "1")
    event = checker.recv()
    assert_error(event, "-", "-", errno.EINVAL)


@pytest.mark.parametrize("count", [1, 16, 32, 64, 128])
def test_concurrency(tmpdir, checker, count):
    paths = {}
    for i in range(count):
        path = tmpdir.join("file-%03d" % i)
        path.write("x")
        paths[str(path)] = "created"

    start = time.time()

    # Send start message for each path
    for path in paths:
        checker.send("start", path, "1")
        paths[path] = "starting"

    # Recieve 2 messages for each path (start, then check)
    for i in range(count * 2):
        event = checker.recv()
        assert event.name in ("start", "check")
        assert event.path in paths
        if event.name == "start":
            paths[event.path] = "started"
        elif event.name == "check":
            paths[event.path] = "checked"

    print("checked %d paths in %.6f" % (count, time.time() - start), end=" ")

    for path, status in paths.items():
        assert status == "checked"


@pytest.mark.timeout(5)
@pytest.mark.parametrize("count", [1, 16, 32, 64, 128])
def test_concurrency_delays(tmpdir, checker, count):
    paths = {}
    for i in range(count):
        path = tmpdir.join("file-%03d" % i)
        path.write("x")
        paths[str(path)] = {"start": None, "checks": []}

    # Send start message for each path
    for path in paths:
        checker.send("start", path, "1")
        paths[path]["start"] = time.time()

    # Recieve 2 messages for each path (start, then check)
    for i in range(count * 2):
        event = checker.recv()
        assert event.name in ("start", "check")
        assert event.path in paths

    # Recieve next check messages for each path
    for i in range(count * 2):
        event = checker.recv()
        now = time.time()
        assert event.name == "check"
        assert event.path in paths
        info = paths[event.path]
        check_time = now - info["start"]
        read_delay = float(event.data)
        info["checks"].append((check_time, read_delay))

    delays = []
    for path, info in sorted(paths.items()):
        for i, (check_time, read_delay) in enumerate(info["checks"]):
            expected = i + 1.0
            delays.append(check_time - read_delay - expected)

    delays.sort()
    avg = sum(delays) / len(delays)
    med = delays[len(delays) // 2]
    mn = delays[0]
    mx = delays[-1]
    print("%d paths delay: avg=%f, med=%f, min=%f, max=%f" % (
        count, avg, med, mn, mx), end=" ")
    assert avg < 0.1


# Helpers

def assert_success(event, name, path):
    assert event.name == name
    assert event.path == path
    assert int(event.error) == 0


def assert_error(event, name, path, error):
    assert event.name == name
    assert event.path == path
    assert int(event.error) == error

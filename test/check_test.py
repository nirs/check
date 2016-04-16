# Copyright 2016 Nir Soffer <nsoffer@redhat.com>
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v2 or (at your option) any later version.

import errno
import subprocess
import time
from collections import namedtuple

import pytest


Event = namedtuple("Event", "name,path,error,data")


class Checker(object):

    def __init__(self):
        self.proc = subprocess.Popen(['./check'],
                                stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)

    def send(self, *args):
        msg = " ".join(args) + "\n"
        self.proc.stdin.write(msg)
        self.proc.stdin.flush()

    def recv(self):
        event = self.proc.stdout.readline().strip()
        print event
        args = event.split(None, 3)
        return Event(*args)

    def close(self):
        self.proc.terminate()
        self.proc.wait()


@pytest.yield_fixture
def checker():
    c = Checker()
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


def test_check_repeat(tmpdir, checker):
    path = tmpdir.join("file")
    path.write("x")
    checker.send("start", str(path), "1")
    start = time.time()
    checker.recv()  # started
    checker.recv()  # first check
    for i in range(1, 6):
        event = checker.recv()
        assert round(time.time() - start, 1) == i * 1.0
        assert_success(event, "check", str(path))


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
    # This suceeds now, maybe it should fail?
    assert_success(event, "check", str(path))
    assert float(event.data) > 0.0


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


# Helpers

def assert_success(event, name, path):
    assert event.name == name
    assert event.path == path
    assert int(event.error) == 0


def assert_error(event, name, path, error):
    assert event.name == name
    assert event.path == path
    assert int(event.error) == error

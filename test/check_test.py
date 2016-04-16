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



@pytest.yield_fixture
def process():
    proc = subprocess.Popen(['./check'],
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    yield proc
    proc.terminate()
    proc.wait()


def test_start_file(tmpdir, process):
    path = tmpdir.join("file")
    path.write("x")
    process.stdin.write("start %s 1\n" % path)
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    assert event == "start %s 0 started" % path
    event = process.stdout.readline().strip()
    print event
    name, path, error, delay = event.split(None, 3)
    assert name == "check"
    assert path == path
    assert error == "0"
    assert float(delay) > 0.0


def test_stop_file(tmpdir, process):
    path = tmpdir.join("file")
    path.write("x")
    process.stdin.write("start %s 1\n" % path)
    process.stdin.flush()
    print process.stdout.readline().strip()  # started
    print process.stdout.readline().strip()  # first check
    process.stdin.write("stop %s\n" % path)
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    name, path, error, desc = event.split(None, 3)
    assert name == "stop"
    assert path == path
    assert error == "0"
    assert desc == "stopped"


def test_check_repeat(tmpdir, process):
    path = tmpdir.join("file")
    path.write("x")
    process.stdin.write("start %s 1\n" % path)
    process.stdin.flush()
    start = time.time()
    print process.stdout.readline().strip()  # started
    print process.stdout.readline().strip()  # first check
    for i in range(1, 6):
        event = process.stdout.readline().strip()
        assert round(time.time() - start, 1) == i * 1.0
        print event
        name, path, error, delay = event.split(None, 3)
        assert name == "check"


def test_start_missing_file(process):
    process.stdin.write("start nosuchfile 1\n")
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    assert event == "start nosuchfile 0 started"
    event = process.stdout.readline().strip()
    print event
    name, path, error, desc = event.split(None, 3)
    assert name == "check"
    assert path == path
    assert int(error) == errno.ENOENT


def test_check_file_appear(tmpdir, process):
    path = tmpdir.join("file")
    process.stdin.write("start %s 1\n" % path)
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    event = process.stdout.readline().strip()
    print event
    path.write("x")
    event = process.stdout.readline().strip()
    print event
    name, path, error, delay = event.split(None, 3)
    assert name == "check"
    assert path == path
    assert error == "0"
    assert float(delay) > 0.0


def test_check_file_disapear(tmpdir, process):
    path = tmpdir.join("file")
    path.write("x")
    process.stdin.write("start %s 1\n" % path)
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    event = process.stdout.readline().strip()
    print event
    path.remove()
    # This suceeds now, maybe it should fail?
    event = process.stdout.readline().strip()
    print event
    name, path, error, delay = event.split(None, 3)
    assert name == "check"
    assert path == path
    assert error == "0"
    assert float(delay) > 0.0


def test_start_twice(tmpdir, process):
    path = tmpdir.join("file")
    path.write("x")
    process.stdin.write("start %s 1\n" % path)
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    event = process.stdout.readline().strip()
    print event
    process.stdin.write("start %s 1\n" % path)
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    name, path, error, desc = event.split(None, 3)
    assert name == "start"
    assert path == path
    assert int(error) == errno.EEXIST


def test_stop_twice(tmpdir, process):
    path = tmpdir.join("file")
    path.write("x")
    process.stdin.write("start %s 1\n" % path)
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    event = process.stdout.readline().strip()
    print event
    process.stdin.write("stop %s\n" % path)
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    process.stdin.write("stop %s\n" % path)
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    name, path, error, desc = event.split(None, 3)
    assert name == "stop"
    assert path == path
    assert int(error) == errno.ENOENT


def test_start_no_path(process):
    process.stdin.write("start\n")
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    name, path, error, desc = event.split(None, 3)
    assert name == "start"
    assert int(error) == errno.EINVAL


def test_start_no_interval(process):
    process.stdin.write("start nosuchpath\n")
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    name, path, error, desc = event.split(None, 3)
    assert name == "start"
    assert int(error) == errno.EINVAL


def test_stop_no_path(process):
    process.stdin.write("stop\n")
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    name, path, error, desc = event.split(None, 3)
    assert name == "stop"
    assert int(error) == errno.EINVAL


def test_uknown_cmd(process):
    process.stdin.write("unknown path 1\n")
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    name, path, error, desc = event.split(None, 3)
    assert int(error) == errno.EINVAL


def test_empty_cmd(process):
    process.stdin.write("\n")
    process.stdin.flush()
    event = process.stdout.readline().strip()
    print event
    name, path, error, desc = event.split(None, 3)
    assert int(error) == errno.EINVAL

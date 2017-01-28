# Copyright 2016 Nir Soffer <nsoffer@redhat.com>
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v2 or (at your option) any later version.

import glob
import random
import subprocess
import signal
import sys
import time

running = True

def terminate(signo, frame):
    global running
    print "received signal %d" % signo
    running = False

signal.signal(signal.SIGHUP, signal.SIG_IGN)
signal.signal(signal.SIGINT, terminate)
signal.signal(signal.SIGTERM, terminate)

interval = int(sys.argv[1])
cmd = sys.argv[2:]

with open("check.log", "a") as log:
    # Use current stdout to consume process output
    p = subprocess.Popen(cmd,
                         stdin=subprocess.PIPE,
                         stderr=log)
    paths = glob.glob("/rhev/data-center/mnt/*/*/dom_md/metadata")
    for path in paths:
        p.stdin.write("start %s %d\n" % (path, interval))
        p.stdin.flush()
    while running:
        try:
            signal.pause()
        except KeyboardInterrupt:
            pass
    print "terminating"
    p.terminate()
    p.wait()

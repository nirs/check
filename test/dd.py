# Copyright 2016 Nir Soffer <nsoffer@redhat.com>
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v2 or (at your option) any later version.

import os
import subprocess
import sys
import time

if len(sys.argv) != 2:
    print "Usage monitor-dd path"
    sys.exit(2)

cmd = ["dd",
       "if=%s" % sys.argv[1],
       "of=/dev/null",
       "bs=4096",
       "count=1",
       "iflag=direct"]

print "timestamp,latency,runtime"

while True:
    start = time.time()
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()
    end = time.time()
    elapsed = end - start
    status = err.splitlines()[-1]
    # 4096 bytes (4.1 kB) copied, 0.00129433 s, 3.2 MB/s
    latency = status.split(",", 2)[1]
    latency = latency.strip().split(" ", 1)[0]
    print "%.9f,%s,%.9f" % (end, latency, elapsed)
    time.sleep(2)

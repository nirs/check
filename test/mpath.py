# Copyright 2016 Nir Soffer <nsoffer@redhat.com>
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v2 or (at your option) any later version.

import subprocess
import glob

with open("check.log", "a") as log:
    p = subprocess.Popen(["./check", "-d"],
                         stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE,
                         stderr=log)
    paths = glob.glob("/dev/disk/by-id/dm-uuid-mpath-*")
    for path in paths:
        p.stdin.write("start %s 1\n" % path)
    try:
        p.wait()
    except KeyboardInterrupt:
        pass
    for path in paths:
        p.stdin.write("stop %s\n" % path)

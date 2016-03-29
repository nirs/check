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

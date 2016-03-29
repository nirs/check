# Copyright 2016 Nir Soffer <nsoffer@redhat.com>
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v2 or (at your option) any later version.

import pandas as pd
from matplotlib import pyplot as plt

md = pd.read_csv("monitord.csv", index_col=0)
mdd = pd.read_csv("monitor_dd.csv", index_col=0)

plt.figure(figsize=(20, 15))
plt.plot(md.index, md.latency, color="blue", label="libaio - latency")
plt.plot(mdd.index, mdd.latency, color="green", label="dd - latency")
plt.plot(mdd.index, mdd.runtime, color="red", label="dd - runtime")
plt.legend()
plt.yscale("log")
plt.grid(True)
plt.show()

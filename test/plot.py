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

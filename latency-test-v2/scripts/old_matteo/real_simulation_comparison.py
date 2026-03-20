import numpy as np
import matplotlib.pyplot as plt
from os import path
from utils import load_files

base_path = path.dirname(__file__)

simulated = list(load_files(path.join(base_path, "../1d-test/db4.dat")))

simulated = simulated[0][1]


def load_real(path):
    with open(path, "r") as f:
        for line in f:
            yield float(line)


real = np.fromiter((load_real(path.join(base_path, "../real_latencies.dat"))), np.dtypes.Float32DType) / 1000
fig = plt.figure(0)
gs = fig.add_gridspec(2, 2)
ax = gs.subplots(sharex=True)

y1, x1 = np.histogram(simulated, density=True, bins=1000)
y2, x2 = np.histogram(real, density=True, bins=1000)

x1 = x1[:-1]
x2 = x2[:-1]

y3 = np.cumsum(y1) / np.sum(y1)
y4 = np.cumsum(y2) / np.sum(y2)

y5 = 1 - y3
y6 = 1 - y4

ax[0, 0].plot(x1, y1, label="Simulated")
ax[0, 0].plot(x2, y2, label="Real")
ax[0, 0].legend()
ax[0, 0].set_title("PDF")
ax[0, 0].set_xlabel("Latency (µs)")
ax[0, 1].plot(x1, y3, label="Simulated")
ax[0, 1].plot(x2, y4, label="Real")
ax[0, 1].set_title("CDF")
ax[0, 1].set_xlabel("Latency (µs)")
ax[1, 0].plot(x1, y5, label="Simulated")
ax[1, 0].plot(x2, y6, label="Real")
ax[1, 0].set_title("CCDF")
ax[1, 0].set_xlabel("Latency (µs)")
ax[1, 1].table(cellText=[
    [
        row(simulated),
        row(real)
    ] for row in (lambda c: np.round(np.mean(c), 3), lambda c: np.round(np.percentile(c, 99), 3), lambda c: np.round(np.percentile(c, 99.9), 3))
], rowLabels=["Mean", "99th", "99.9th"], colLabels=["Simulated", "Real"], loc="center")
ax[1, 1].axis("off")

plt.show()

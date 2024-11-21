from typing import NamedTuple, Optional
import numpy as np
import matplotlib.pyplot as plt
from utils import get_files

Args = NamedTuple("Args", [
    ("paths", str),
    ("bins", int),
    ("cutoff", float),
    ("type", str),
    ("savePath", Optional[str]),
])


def pdf(latencies: np.array, bins: int, cutoff: float):
    y, x = np.histogram(latencies[latencies < cutoff], bins=bins, density=True)
    return x[1:], y / np.sum(y)


def cdf(latencies: np.array, bins: int, cutoff: float):
    y, x = np.histogram(latencies, bins=bins, density=True)
    return x[:-1], np.cumsum(y) / np.sum(y)


def ccdf(latencies: np.array, bins: int, cutoff: float):
    x, y = cdf(latencies, bins, cutoff)
    return x, 1 - y


GRAPH_TYPES = {
    "pdf": pdf,
    "cdf": cdf,
    "ccdf": ccdf,
}


def main(args: Args):
    plt.figure()
    paths = [file for path in args.paths for file in get_files(path)]
    for path in paths:
        with open(path, "r") as f:
            line = f.readline()
            if line.startswith("#"):
                title = line.lstrip("# ").rstrip()
            latencies = np.array([int(line) for line in f if not line.startswith("MPDU Timeout")]) / 1000

        x, y = GRAPH_TYPES[args.type](latencies, args.bins, args.cutoff)

        plt.plot(x, y, label="\n".join(title[11:-1].split(", ")))
    #plt.legend()
    plt.title(f"{args.type}(cutoff={args.cutoff}, bins={args.bins})")
    plt.show()

    if args.savePath is not None:
        plt.savefig(args.savePath)


if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser(description="Creates graphs based on the latency passed")
    parser.add_argument("--cutoff", type=float, default=10000.0, help="The cutoff value for the latency")
    parser.add_argument("--bins", type=int, default=1000, help="The number of bins")
    parser.add_argument("--savePath", type=str, nargs="?", help="The path where to save the graph")
    parser.add_argument("type", choices=list(GRAPH_TYPES.keys()), help="The type of graph")
    parser.add_argument("paths", type=str, nargs="+", help="The path to the latency files")
    main(Args(**vars(parser.parse_args())))

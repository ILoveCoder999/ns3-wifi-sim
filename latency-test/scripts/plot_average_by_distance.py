from typing import NamedTuple, Optional
import numpy as np
import matplotlib.pyplot as plt
from utils import get_files, Parameters

Args = NamedTuple("Args", [
    ("paths", str),
    ("savePath", Optional[str]),
])


def main(args: Args):
    plt.figure()
    paths = [file for path in args.paths for file in get_files(path)]
    x = []
    y = []
    for path in paths:
        with open(path, "r") as f:
            line = f.readline()
            if line.startswith("#"):
                title = line.lstrip("# ").rstrip()
                parameters = eval(title)
            latencies = np.array([int(line) for line in f if not line.startswith("MPDU Timeout")]) / 1000
        x.append(parameters.apStaDistance)
        y.append(latencies.mean())

    plt.plot(x, y)
    #plt.legend()
    plt.title(f"Average by distance")
    plt.show()

    if args.savePath is not None:
        plt.savefig(args.savePath)


if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser(description="Creates graphs based on the latency passed")
    parser.add_argument("--savePath", type=str, nargs="?", help="The path where to save the graph")
    parser.add_argument("paths", type=str, nargs="+", help="The path to the latency files")
    main(Args(**vars(parser.parse_args())))

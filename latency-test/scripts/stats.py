from typing import NamedTuple
import numpy as np
from utils import Parameters, get_files

Args = NamedTuple("Args", [
    ("paths", str),
])

FUNCTIONS = [np.mean, np.std, np.median, np.min,
             lambda x: np.nanpercentile(x, 99), lambda x: np.nanpercentile(x, 99.9),
             lambda x: np.nanpercentile(x, 99.99), np.max, np.size]
ROW_FORMAT = "{:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10d} {:10d} {:10.3f} {:10.3f}"
HEADER = ("{:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s}"
          .format("Mean", "Std", "Median", "Min", "99%", "99.9%", "99.99%", "Max", "Size", "Interferent", "Distance", "Int. distance"))


def main(args: Args):
    print(HEADER)

    paths = [file for path in args.paths for file in get_files(path)]

    for path in paths:
        try:
            with (open(path, "r") as f):
                line = f.readline()
                if line.startswith("#"):
                    title = line.lstrip("# ").rstrip()
                    p: Parameters = eval(title)
                _ = f.readline()
                latencies = np.array([int(line) for line in f if not line.startswith("MPDU Timeout")]) / 1000
            if latencies.shape[0] == 0:
                print("No data", title)
            else:
                print(ROW_FORMAT.format(*map(lambda func: func(latencies), FUNCTIONS), p.interferenceNodes, p.apStaDistance, p.apInterferentDistanceX))
        except Exception as e:
            print("Error reading", path, f"Interferent={p.interferenceNodes}, apStaDistance={p.apStaDistance}, apInterferentDistanceX={p.apInterferentDistanceX}")
            print(e)


if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser(description="Prints statistics based on latencies")
    parser.add_argument("paths", type=str, nargs="+", help="The path to the latency files")
    main(Args(**vars(parser.parse_args())))

from typing import NamedTuple, Tuple, List, Optional
from itertools import groupby
import numpy as np
from utils import load_files
from utils.V2 import JsonConfig
import matplotlib.pyplot as plt
from pathlib import Path

Args = NamedTuple("Args", [
    ("paths", List[str]),
    ("savePath", Optional[str]),
])


def main(args: Args):
    files: List[Tuple[JsonConfig, np.typing.NDArray[float]]] = [file for path in args.paths for file in load_files(path)]
    last_figure = 0
    for file_key, files_group in groupby(files, lambda f: f[0].interferent):
        sorted_files_group = sorted(files_group, key=lambda f: f[0].sta.position.x)

        x = np.array([f[0].sta.position.x for f in sorted_files_group])
        y1 = np.array([np.nanmean(f[1]) for f in sorted_files_group])
        y2 = np.array([np.nanpercentile(f[1], 99) for f in sorted_files_group])
        y3 = np.array([np.nanpercentile(f[1], 99.9) for f in sorted_files_group])
        fig = plt.figure(last_figure)
        title = "Latency mean over distance"
        if len(file_key) == 0:
            title += "\n(No interferent)"
        else:
            title += f"\n({len(file_key)} interferent at {', '.join(map(lambda x: str(x.position.x), file_key))} meters)"
        fig.suptitle(title)
        gs = fig.add_gridspec(2)
        ax = gs.subplots(sharex=True)
        ax[0].plot(x, y1, label="Mean")
        ax[0].set_ylabel("Latency (µs)")
        ax[0].legend()
        ax[1].plot(x, y2, label="99th percentile")
        ax[1].plot(x, y3, label="99.9th percentile")
        ax[1].set_xlabel("Distance from AP (m)")
        ax[1].set_ylabel("Latency (µs)")
        ax[1].legend()
        if args.savePath is not None:
            fig.savefig(Path(args.savePath).joinpath(f"Figure_{last_figure}.png"))
            print("Saved", f"Figure_{last_figure}")
        else:
            plt.show()
        last_figure += 1


if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser(description="Prints statistics based on latencies")
    parser.add_argument("paths", type=str, nargs="+", help="The path to the latency files")
    parser.add_argument("--savePath", type=str, default=None, help="The path to which save the images")
    main(Args(**vars(parser.parse_args())))

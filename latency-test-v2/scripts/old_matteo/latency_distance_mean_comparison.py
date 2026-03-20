from typing import NamedTuple, Tuple, List, Optional
from itertools import groupby
import numpy as np
from utils import load_files
from utils.V2 import JsonConfig
import matplotlib.pyplot as plt
from pathlib import Path

Args = NamedTuple("Args", [
    ("paths", List[str]),
    ("previousInterferent", bool),
    ("savePath", Optional[str]),
])


def main(args: Args):
    files: List[Tuple[JsonConfig, np.typing.NDArray[float]]] = [file for path in args.paths for file in load_files(path)]
    last_figure = 0

    def sort_and_concatenate(g):
        return g[0], sorted(map(lambda f: (f[0], np.concatenate((*map(lambda ff: ff[1], f[1]), ))), groupby(sorted(g[1], key=lambda f: f[0].sta.position.x), lambda f: f[0].sta.position.x)), key=lambda f: f[0])

    iterator = list(map(sort_and_concatenate, groupby(files, lambda f: f[0].interferent)))
    prev = iterator[0]

    def key_to_name(key):
        if len(key) == 0:
            return "(No interferer)"
        else:
            return f"({len(key)} interferer at {', '.join(map(lambda x: str(x.position.x), key))} meters)"

    for file_key, sorted_files_group in iterator[1:]:
        if args.previousInterferent:
            try:
                prev_file_key, sorted_prev_files_group = next(g for g in iterator if g[0] == file_key[:-2])
            except StopIteration:
                prev_file_key, sorted_prev_files_group = prev
        else:
            prev_file_key = prev[0]
            sorted_prev_files_group = prev[1]

        x1 = np.array([f[0] for f in sorted_prev_files_group])
        x2 = np.array([f[0] for f in sorted_files_group])
        y1 = np.array([np.nanmean(f[1]) for f in sorted_prev_files_group])
        y2 = np.array([np.nanmean(f[1]) for f in sorted_files_group])
        y3 = np.array([np.nanpercentile(f[1], 99) for f in sorted_prev_files_group])
        y4 = np.array([np.nanpercentile(f[1], 99) for f in sorted_files_group])
        y5 = np.array([np.nanpercentile(f[1], 99.9) for f in sorted_prev_files_group])
        y6 = np.array([np.nanpercentile(f[1], 99.9) for f in sorted_files_group])
        fig = plt.figure(last_figure)
        title = f"Latency over distance\n{key_to_name(prev_file_key)}\n{key_to_name(file_key)}"
        fig.suptitle(title)
        gs = fig.add_gridspec(3)
        ax = gs.subplots(sharex=True)
        ax[0].plot(x1, y1, '--', label=f"Mean {key_to_name(prev_file_key)}")
        ax[0].plot(x2, y2, label=f"Mean {key_to_name(file_key)}")
        ax[0].set_ylabel("Latency (µs)")
        ax[0].legend()
        ax[1].plot(x1, y3, '--', label=f"99th percentile {key_to_name(prev_file_key)}")
        ax[1].plot(x2, y4, label=f"99th percentile {key_to_name(file_key)}")
        ax[1].set_ylabel("Latency (µs)")
        ax[1].legend()
        ax[2].plot(x1, y5, '--', label=f"99.9th percentile {key_to_name(prev_file_key)}")
        ax[2].plot(x2, y6, label=f"99.9th percentile {key_to_name(file_key)}")
        ax[2].set_xlabel("Distance from AP (m)")
        ax[2].set_ylabel("Latency (µs)")
        ax[2].legend()
        if args.savePath is not None:
            fig.savefig(Path(args.savePath).joinpath(f"Figure_{last_figure}.svg"))
            print("Saved", f"Figure_{last_figure}")
            plt.close(fig)
        else:
            plt.show()
            plt.close(fig)
        last_figure += 1
        prev = (file_key, sorted_files_group)


if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser(description="Prints statistics based on latencies")
    parser.add_argument("paths", type=str, nargs="+", help="The path to the latency files")
    parser.add_argument("--previousInterferent", default=False, action="store_true", help="If set the comparison is between the previous case of interferents")
    parser.add_argument("--savePath", type=str, default=None, help="The path to which save the images")
    main(Args(**vars(parser.parse_args())))

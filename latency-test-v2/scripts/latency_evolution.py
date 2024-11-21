from typing import NamedTuple, Tuple, List, Optional
from itertools import groupby
import numpy as np
from utils import load_files
from utils.V2 import JsonConfig
import matplotlib as mpl
import matplotlib.pyplot as plt
from pathlib import Path

Args = NamedTuple("Args", [
    ("paths", List[str]),
    ("clipMin", Optional[float]),
    ("clipMax", Optional[float]),
    ("savePath", Optional[str]),
])


def main(args: Args):
    files: List[Tuple[JsonConfig, np.typing.NDArray[float]]] = [file for path in args.paths for file in load_files(path)]
    last_figure = 0
    for file_key, files_group in groupby(files, lambda f: f[0].interferent):
        sorted_files_group = sorted(files_group, key=lambda f: f[0].sta.position.x)
        x = np.arange(0.0, float(sorted_files_group[0][0].simulationTime), sorted_files_group[0][0].sta.packetInterval)
        y = np.array([f[0].sta.position.x for f in sorted_files_group])
        z = np.array([
            np.array(f[1])
            for f in sorted_files_group
        ])[:-1, :]
        if args.clipMin is not None:
            z = z.clip(np.percentile(z, args.clipMin), None)
        if args.clipMax is not None:
            z = z.clip(None, np.percentile(z, args.clipMax))
        plt.figure(last_figure)
        title = "Latency evolution"
        if len(file_key) == 0:
            title += "\n(No interferent)"
        else:
            title += f"\n({len(file_key)} interferent at {', '.join(map(lambda x: str(x.position.x), file_key))} meters)"
        plt.title(title)
        cmap = mpl.colormaps.get_cmap('viridis')
        cmap.set_bad(color='red')
        plt.pcolormesh(x, y, z, norm="log", cmap=cmap)
        plt.xlabel("Time (s)")
        plt.ylabel("Distance from AP (m)")
        cbar = plt.colorbar()
        cbar.set_label("Latency (µs)")
        if args.savePath is not None:
            plt.savefig(Path(args.savePath).joinpath(f"Figure_{last_figure}.png"))
            print("Saved", f"Figure_{last_figure}")
        else:
            plt.show()
        last_figure += 1


if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser(description="Prints statistics based on latencies")
    parser.add_argument("paths", type=str, nargs="+", help="The path to the latency files")
    parser.add_argument("--clipMin", type=float, default=None, help="The percentage on the curve where to clip the minimum")
    parser.add_argument("--clipMax", type=float, default=None, help="The percentage on the curve where to clip the maximum")
    parser.add_argument("--savePath", type=str, default=None, help="The path to which save the images")
    main(Args(**vars(parser.parse_args())))
    # for i in range(0, 990, 50):
    #    print("---", i, "---")
    #    main(Args([f"./1d-test/db{i}.dat" for i in range(i, i + 49)], None, None))
    # main(Args([f"./1d-test/db{i}.dat" for i in range(250, 299)], None, None))

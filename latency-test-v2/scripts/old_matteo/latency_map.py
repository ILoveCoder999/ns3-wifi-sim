from typing import NamedTuple, List, Iterable, Tuple, Generator, Any, Optional
import numpy as np
from utils import load_files, PacketInfo
from utils.V2 import JsonConfig, InterfererConfig
import matplotlib as mpl
import matplotlib.pyplot as plt
from itertools import groupby
from pathlib import Path

Args = NamedTuple("Args", [
    ("paths", List[str]),
    ("clipMin", Optional[float]),
    ("clipMax", Optional[float]),
    ("savePath", Optional[str]),
])


def group_by_interferer_nodes(iterable: Iterable[Tuple[JsonConfig, List[PacketInfo], int]]) \
        -> Generator[Tuple[List[InterfererConfig], Iterable[Tuple[JsonConfig, List[PacketInfo], int]]], Any, None]:
    def key(f: Tuple[JsonConfig, List[PacketInfo], int]):
        return f[0].interfererNodes
    for k, g in groupby(iterable, key=key):
        yield k, g


def build_x(iterable: Iterable[Tuple[JsonConfig, List[PacketInfo], int]]):
    def key(f: Tuple[JsonConfig, List[PacketInfo], int]):
        return f[0].staNode.position.x
    return np.array([f[0].staNode.position.x for f in sorted(iterable, key=key)])


def build_y(iterable: Iterable[Tuple[JsonConfig, List[PacketInfo], int]]):
    def key(f: Tuple[JsonConfig, List[PacketInfo], int]):
        return f[0].staNode.position.y
    return np.array([f[0].staNode.position.y for f in sorted(iterable, key=key)])


def build_z(iterable: Iterable[Tuple[JsonConfig, List[PacketInfo], int]]):
    def sort_key(f: Tuple[JsonConfig, List[PacketInfo], int]):
        return f[0].staNode.position.x, f[0].staNode.position.y

    def group_by_x(_iterable: Iterable[Tuple[JsonConfig, List[PacketInfo], int]])\
            -> Generator[Iterable[Tuple[JsonConfig, List[PacketInfo], int]], Any, None]:
        def key(f: Tuple[JsonConfig, List[PacketInfo], int]):
            return f[0].staNode.position.x
        for _, g in groupby(_iterable, key=key):
            yield g

    return np.array([
        np.mean([t.latency for t in _tuple[1]])
        if len(_tuple[1]) > 0 else np.nan
        for _iterable in group_by_x(sorted(iterable, key=sort_key))
        for _tuple in _iterable
    ])[:-1, :]


def main(args: Args):
    files = list(load_files(args.paths))
    last_figure = 0

    for file_key, files_group in group_by_interferer_nodes(files):

        x = build_x(files_group)
        y = build_y(files_group)
        z = build_z(files_group)
        if args.clipMin is not None:
            z = z.clip(np.percentile(z, args.clipMin), None)
        if args.clipMax is not None:
            z = z.clip(None, np.percentile(z, args.clipMax))
        plt.figure(last_figure)
        title = "Latency map"
        if len(file_key) == 0:
            title += "\n(No interferer)"
        else:
            title += f"\n({len(file_key)} interferer at {', '.join(map(lambda interferer: str((interferer.position.x, interferer.position.y)), file_key))})"
        plt.title(title)
        cmap = mpl.colormaps.get_cmap('viridis')
        cmap.set_bad(color='red')
        plt.pcolormesh(x, y, z, cmap=cmap)
        plt.xlabel("X (m)")
        plt.ylabel("Y (m)")
        cbar = plt.colorbar()
        cbar.set_label("Mean latency (µs)")
        if args.savePath is not None:
            plt.savefig(Path(args.savePath).joinpath(f"Figure_{last_figure}.png"))
            print("Saved", f"Figure_{last_figure}")
        else:
            plt.show()
        last_figure += 1


if __name__ == "__main__":
    from argparse import ArgumentParser
    parser = ArgumentParser(description="Prints the latency map")
    parser.add_argument("paths", type=str, nargs="+", help="The path to the latency files")
    parser.add_argument("--clipMin", type=float, default=None, help="The percentage on the curve where to clip the minimum")
    parser.add_argument("--clipMax", type=float, default=None, help="The percentage on the curve where to clip the maximum")
    parser.add_argument("--savePath", type=str, default=None, help="The path to which save the images")
    main(Args(**vars(parser.parse_args())))

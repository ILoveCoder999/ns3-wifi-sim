from math import sqrt
from typing import NamedTuple, List
import numpy as np
from utils import load_files
from utils.V2 import JsonConfig
import pandas as pd

Args = NamedTuple("Args", [
    ("paths", List[str]),
])

FUNCTIONS = [np.mean, np.std, np.median, np.min,
             lambda x: np.nanpercentile(x, 99), lambda x: np.nanpercentile(x, 99.9),
             lambda x: np.nanpercentile(x, 99.99), np.max, np.size]
ROW_FORMAT = "{:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10.3f} {:10d} {:10d} {:10.3f}"
HEADER = ("{:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s} {:^10s}"
          .format("Mean", "Std", "Median", "Min", "99%", "99.9%", "99.99%", "Max", "Size", "Interferent", "Distance"))


def make_title(config: JsonConfig) -> str:
    apSta = next(filter(lambda x: x.ssid == config.staNode.ssid, config.apNodes))
    apStaDistance = sqrt((apSta.position.x - config.staNode.position.x) ** 2 + (apSta.position.y - config.staNode.position.y) ** 2)
    return f"{len(config.interfererNodes)} int. STA {apStaDistance} m"


def main(args: Args):
    files = {
        make_title(config): pd.DataFrame([
            {
                "latency": info.latency / 1000.0,
                "acked": info.acked,
                "tot_transmissions": len(info.retransmissions) + 1
            }
            for info in packets
        ])
        for config, packets, elapsed in load_files(args.paths)
    }

    quantity = pd.DataFrame(map(lambda row: [*row, (row[1] / row[0]) * 100, (row[2] / row[0]) * 100], [
        [ lines.shape[0], lines[lines["acked"]].shape[0], lines[~lines["acked"]].shape[0] ] for lines in files.values()
    ]), files.keys(), [
                                "All",
                                "Acked",
                                "Not acked",
                                "Acked %",
                                "Not acked %"
                            ]).round(decimals=1)

    print(quantity)

    print("**** Statistics ****")
    # Usa un dataframe per avere una stampa decente
    major_columns = [ "Average", "Std. dev.", "Min", "P_5", "P_{10}", "P_{95}", "P_{99}", "P_{99.9}", "P_{99.99}", "Max" ]

    def empty_protection(func):
        def wrapper(series):
            if series.size == 0:
                return np.nan
            return func(series)
        return wrapper

    functions = [ np.nanmean, np.nanstd, np.nanmin, lambda x: np.nanpercentile(x, 5), lambda x: np.nanpercentile(x, 10), lambda x: np.nanpercentile(x, 95), lambda x: np.nanpercentile(x, 99), lambda x: np.nanpercentile(x, 99.9), lambda x: np.nanpercentile(x, 99.99), np.nanmax ]
    functions = [ empty_protection(f) for f in functions ]
    minor_columns = [ "All", "Acked", "Not acked" ]

    df = pd.DataFrame([
        [ function(lat) for function in functions for lat in [lines["latency"], lines[lines["acked"]]["latency"], lines[~lines["acked"]]["latency"]] ] for lines in files.values()
    ],
        pd.MultiIndex.from_product([["Files"], files.keys()]),
        pd.MultiIndex.from_product([major_columns, minor_columns])
    ).round(decimals=1)
    # print(df)

    df2 = pd.DataFrame([
        [ function(lat) for function in functions ] for lines in files.values() for lat in [lines["latency"], lines[lines["acked"]]["latency"], lines[~lines["acked"]]["latency"]]
    ],
        pd.MultiIndex.from_product([files.keys(), minor_columns]),
        major_columns
    ).round(decimals=1)
    print(df2)

    print("**** Statistics on retrasmissions ****")

    max_transmissions = {
        file: lines["tot_transmissions"].max() for file, lines in files.items()
    }

    filtered = {
        file: {
            t: files[file][files[file]["tot_transmissions"] == t + 1] for t in range(transmissions)
        } for file, transmissions in max_transmissions.items()
    }

    df = pd.DataFrame([
        [ lines.shape[0], (lines.shape[0] / files[file].shape[0]) * 100, lines["latency"].mean(), lines["latency"].std(), lines["latency"].min(), lines["latency"].max() ] for file, transmissions in filtered.items() for t, lines in transmissions.items()
    ],
        pd.MultiIndex.from_tuples([
            (file, f"{t + 1} {'transmission' if t == 0 else 'transmissions'}") for file, transmissions in max_transmissions.items() for t in range(transmissions)
        ]),
        [ "Absolute value", "% over total", "Avg. latency", "Std. dev. latency", "Min latency", "Max latency"]
    )
    df["% over total"] = df["% over total"].round(5)
    df["Avg. latency"] = df["Avg. latency"].round(1)
    df["Std. dev. latency"] = df["Std. dev. latency"].round(1)
    print(df)

    df = pd.DataFrame([
        [ lines["tot_transmissions"].mean(), lines["tot_transmissions"].std() ] for lines in files.values()
    ],
        files.keys(),
        ["Average transmissions", "Std. dev."]
    ).round(decimals=5)
    print(df)

    print("*** Over 99.99 ***")
    pd.set_option('display.max_rows', None)
    filtered = {
        file: lines[lines["latency"] > np.nanpercentile(lines["latency"], 99.99)]
        for file, lines in files.items()
    }
    df = pd.DataFrame([
        [line["latency"], line["acked"], line["tot_transmissions"]] for lines in filtered.values() for _, line in lines.iterrows()
    ],
        pd.MultiIndex.from_tuples([
            (file, index) for file, lines in filtered.items() for index, _ in lines.iterrows()
        ]),
        ["Latency", "Acked", "Tot. retransmissions"]
    )
    print(df)


if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser(description="Prints statistics based on latencies")
    parser.add_argument("paths", type=str, nargs="+", help="The path to the latency files")
    main(Args(**vars(parser.parse_args())))

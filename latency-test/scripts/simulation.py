from pathlib import Path
from typing import NamedTuple, Optional, TextIO, Tuple
from dataclasses import asdict
import subprocess
from os import path, rename
from concurrent.futures import ProcessPoolExecutor, as_completed
import pandas as pd
import json
from time import time
from utils import Parameters
from utils.V2 import JsonConfig

Args = NamedTuple("Args", [
    ("verbose", bool),
    ("out", str),
    ("start", int),
    ("skip", int),
    ("length", Optional[int]),
    ("jsonConfig", bool),
    ("reverse", bool),
    ("cores", Optional[int]),
    ("simulations", TextIO),
])

SimulationArgs = NamedTuple("SimulationArgs", [
    ("id", int),
    ("verbose", bool),
    ("outputFilePath", str),
    ("parameters", dict),
])


ROW_FORMAT = "{:10.3f} {}"
HEADER = ("{:^10s} {:^30s}"
          .format("Time (s)", "Parameters"))
LATENCY_FILE = "db{:d}.dat"
REPORT_FILE = "{}/report.txt"


def run_simulation(args: SimulationArgs) -> Tuple[SimulationArgs, float]:
    if path.exists(args.outputFilePath):
        print(f"({args.id}) Skipping simulation. File already exists")
        return args, 0
    t1 = time()
    if args.verbose:
        print(f"({args.id}) [{t1}] Starting simulation")
    #print(*map(lambda p: f"--{p[0]}={p[1]}", filter(lambda p: p is not None, args.parameters.items())))
    with open(f"{args.outputFilePath}.tmp", "w") as f:
        print(f"# {args.parameters}", file=f)
        f.flush()
        _, err = subprocess.Popen([
            path.abspath("../build/latency-test/ns3-dev-latency-test-optimized"),
            *map(lambda p: f"--{p[0]}={p[1]}", filter(lambda p: p is not None, args.parameters.items())),
        ], stdout=f, stderr=subprocess.PIPE, encoding="utf-8").communicate()
    t2 = time()
    if args.verbose:
        print(f"({args.id}) [{t2}] Finishing simulation")
    if err != '':
        raise Exception(err)
    rename(f"{args.outputFilePath}.tmp", args.outputFilePath)
    return args, t2 - t1


def main(args: Args):
    if args.jsonConfig:
        experiments = [JsonConfig(**obj) for obj in json.load(args.simulations)]
    else:
        experiments = [Parameters(**obj) for obj in json.load(args.simulations)]

    Path(args.out).mkdir(parents=True, exist_ok=True)

    if args.reverse:
        experiments = list(reversed(list(enumerate(experiments[args.skip:][:args.length], start=args.start))))
    else:
        experiments = list(enumerate(experiments[args.skip:][:args.length], start=args.start))

    if not path.exists(REPORT_FILE.format(args.out)):
        df = pd.DataFrame([
            {
                "Execution time": 0.0,
                "Configuration": json.dumps(asdict(experiment))
            } for _, experiment in experiments
        ])
        df.index += args.start
        df.to_csv(REPORT_FILE.format(args.out), sep=" ", index_label="Id")

    if args.verbose:
        print("Simulation runner")
        print("Running the following simulations")
        for i, experiment in experiments:
            print(experiment, "->", Path(args.out).joinpath(LATENCY_FILE.format(i)))

    with ProcessPoolExecutor(max_workers=args.cores) as executor:

        futures = [
            executor.submit(run_simulation,
                            SimulationArgs(i,
                                           args.verbose,
                                           str(Path(args.out).joinpath(LATENCY_FILE.format(i))),
                                           {"jsonConfig": json.dumps(asdict(experiment), separators=(',', ':'))} if args.jsonConfig else experiment.__dict__,
                                           )
                            )
            for i, experiment in experiments
        ]

        for future in as_completed(futures):
            try:
                ex_args, ex_time = future.result()
                if ex_time == 0:
                    continue
                df = pd.read_csv(REPORT_FILE.format(args.out), sep=r"\s+", index_col=0)
                df.loc[ex_args.id] = {
                    "Execution time": ex_time,
                    "Configuration": ex_args.parameters["jsonConfig"] if args.jsonConfig else json.dumps(ex_args.parameters)
                }
                df.to_csv(REPORT_FILE.format(args.out), sep=" ", index_label="Id")
            except Exception as e:
                # Dividere eccezioni in modo da recuperare in caso di problemi con pandas
                print("Error: ", e)

    df = pd.read_csv(REPORT_FILE.format(args.out), sep=r"\s+", index_col=0)
    print(df)


if __name__ == '__main__':
    from argparse import ArgumentParser, FileType
    parser = ArgumentParser(description="Executes some simulations and collects results")
    parser.add_argument("--verbose", "-v", action="store_true", default=False, help="Verbose output")
    parser.add_argument("--out", "-o", type=str, default=".", help="Output folder path")
    parser.add_argument("--start", "-s", type=int, default=0, help="Start number of the dat files")
    parser.add_argument("--skip", type=int, default=0, help="Start number of simulations to skip")
    parser.add_argument("--length", type=int, default=None, help="The number of simulations to run. All if not set")
    parser.add_argument("--reverse", default=False, action="store_true", help="If set, executes simulations in reverse")
    parser.add_argument("--jsonConfig", default=False, action="store_true", help="If set, interprets the parameters to be passed as jsonConfig")
    parser.add_argument("--cores", type=int, default=None, help="The number of cores to use for the simulations")
    parser.add_argument("simulations", type=FileType(), help="The file containing the simulations configuration")
    main(Args(**vars(parser.parse_args())))


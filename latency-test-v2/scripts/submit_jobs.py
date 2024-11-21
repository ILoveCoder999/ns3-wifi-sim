from os import path
import subprocess
import json
from dataclasses import dataclass, asdict
from typing import TextIO, List
from pathlib import Path
from utils.V2 import JsonConfig

LATENCY_FILE = "db{:d}.dat"
STDOUT_FILE = "out{:d}.txt"
STDERR_FILE = "err{:d}.txt"

@dataclass
class Args:
    batch_file: TextIO
    batch_size: int
    out_dir: str


def main(args: Args):
    simulations: List[JsonConfig] = [JsonConfig(**c) for c in json.load(args.batch_file) ]

    Path(args.out_dir).mkdir(parents=True, exist_ok=True)

    if args.batch_size == -1:
        args.batch_size = len(simulations)

    for i, sim in enumerate(simulations):
        if args.batch_size <= 0:
            break
        executable = Path(__file__).parent.joinpath("../../build/latency-test-v2/ns3-dev-latency-test-v2-optimized").resolve()
        config = json.dumps(asdict(sim), separators=(',', ':'))
        out_file = Path(args.out_dir).joinpath(LATENCY_FILE.format(i))
        stdout = Path(args.out_dir).joinpath(STDOUT_FILE.format(i))
        stderr = Path(args.out_dir).joinpath(STDERR_FILE.format(i))
        if out_file.exists():
            continue

        script = f"""#BSUB -n 1
#BSUB -q ext_batch
#BSUB -J ns-3-sim-{i}
#BSUB -eo {stderr}
#BSUB -oo {stdout}

{executable} '{config}' {out_file}.tmp
mv {out_file}.tmp {out_file}
"""
        subprocess.run([
            "bsub"
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, input=script, encoding="ascii")
        args.batch_size -= 1


if __name__ == "__main__":
    from argparse import ArgumentParser, FileType
    parser = ArgumentParser(description="Runs a batch of simulations")
    parser.add_argument("batch_file", type=FileType(), help="The job file")
    parser.add_argument("batch_size", type=int, default=-1, help="The number of jobs to include in the batch. -1 for all")
    parser.add_argument("out_dir", type=str, help="The path to the directory for output files")
    main(Args(**vars(parser.parse_args())))

import json
import os
import re
import numpy as np
from typing import List, Union
from functools import cmp_to_key
from .V2 import JsonConfig, PacketInfo

FILENAME_PATTERN = re.compile(r"db(?P<db_number>\d+)\.dat")


def get_files(path: str | os.PathLike[str] | List[str | os.PathLike[str]]) -> List[str]:
    def by_db_number(path1, path2):
        m1 = FILENAME_PATTERN.match(os.path.basename(path1))
        m2 = FILENAME_PATTERN.match(os.path.basename(path2))
        db1 = int(m1.group("db_number"))
        db2 = int(m2.group("db_number"))
        if db1 < db2:
            return -1
        if db1 > db2:
            return 1
        return 0

    if isinstance(path, list):
        return sorted((f for file in path for f in get_files(file)), key=cmp_to_key(by_db_number))
    if os.path.isfile(path) and FILENAME_PATTERN.match(os.path.basename(path)):
        return [path]
    res = []

    for root, dirnames, filenames in os.walk(path):
        for filename in filenames:
            if FILENAME_PATTERN.match(filename):
                res.append(os.path.join(root, filename))
    return sorted(res, key=cmp_to_key(by_db_number))


def load_files(path: str | os.PathLike[str] | List[str | os.PathLike[str]]):
    files = get_files(path)
    for file in files:
        with (open(file, "r") as f):
            data: list = json.load(f)
        p = JsonConfig(**data[0])
        latencies = [PacketInfo(**line) for line in data[1:-1]]
        t: int = data[-1]["elapsed_seconds"]
        yield p, latencies, t

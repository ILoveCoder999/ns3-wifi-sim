import json
import os
import re
import numpy as np
from typing import List, Union
from functools import cmp_to_key
from dataclasses import dataclass
from .V2 import JsonConfig

FILENAME_PATTERN = re.compile(r"db(?P<db_number>\d+)\.dat")


@dataclass
class Parameters:
    interferenceNodes: int = 0
    apStaDistance: float = 20
    apInterferentDistanceX: float = 10
    apInterferentDistanceY: float = 5
    payloadSize: int = 50
    packetInterval: float = 0.5
    simulationTime: float = 10
    interferenceOffTime: str = "ns3::ExponentialRandomVariable[Mean=0.25|Bound=10]"
    interferenceBurstSize: str = "ns3::ExponentialRandomVariable[Mean=100|Bound=500]"
    staRemoteStationManager: str = "ns3::IdealWifiManager"
    interferenceRemoteStationManager: str = "ns3::IdealWifiManager"
    staRtsCtsThreshold: int = 4692480
    interferenceRtsCtsThreshold: int = 4692480
    staDataMode: str = "OfdmRate6Mbps"
    interferenceDataMode: str = "OfdmRate6Mbps"


def get_files(path:  Union[str, os.PathLike[str]]) -> List[str]:
    if os.path.isfile(path) and FILENAME_PATTERN.match(os.path.basename(path)):
        return [path]
    res = []

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

    for root, dirnames, filenames in os.walk(path):
        for filename in filenames:
            if FILENAME_PATTERN.match(filename):
                res.append(os.path.join(root, filename))
    return sorted(res, key=cmp_to_key(by_db_number))


def load_files(path: Union[str, os.PathLike[str]]):
    timeout_regex = re.compile(r"MPDU Timeout \((?P<seq_num>\d+)\) (?P<timeout>\d+)")
    files = get_files(path)
    for file in files:
        with (open(file, "r") as f):
            line = f.readline()
            if line.startswith("#"):
                title = line.lstrip("# ").rstrip()
                p = json.loads(title.replace("\"", "\\\"").replace("'", "\""))
                if 'jsonConfig' in p:
                    p = JsonConfig(**json.loads(p["jsonConfig"]))
                else:
                    p = Parameters(**p)
            latencies = []
            prev_seq_num = -1
            prev_timeout = 0
            for line in f:
                m = timeout_regex.match(line)
                if m is not None:
                    if int(m.group("seq_num")) > prev_seq_num:
                        if prev_timeout != 0:
                            latencies += [np.nan for _ in range(int(m.group("seq_num")) - prev_seq_num)]
                        prev_seq_num = int(m.group("seq_num"))
                    prev_timeout = int(m.group("timeout"))
                else:
                    latency = int(line)
                    if latency < prev_timeout:
                        latencies += [np.nan for _ in range(prev_seq_num - len(latencies) + 1)]
                    prev_timeout = 0
                    latencies.append(latency)
            latencies = np.array(latencies)[1:] / 1000
        yield p, latencies

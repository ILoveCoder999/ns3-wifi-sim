import json
import os.path
from typing import List, get_args, get_origin
from dataclasses import dataclass, field, is_dataclass


def nested_dataclass(*args, **kwargs):
    def wrapper(cls):
        cls = dataclass(cls, **kwargs)
        original_init = cls.__init__

        def __init__(self, *args, **kwargs):
            for name, value in kwargs.items():
                field_type = cls.__annotations__.get(name, None)
                if is_dataclass(field_type) and isinstance(value, dict):
                    new_obj = field_type(**value)
                    kwargs[name] = new_obj
                if get_origin(field_type) == list and is_dataclass(get_args(field_type)[0]) and isinstance(value, list):
                    kwargs[name] = [v if isinstance(v, get_args(field_type)[0]) else get_args(field_type)[0](**v) for v in value]
            original_init(self, *args, **kwargs)
        cls.__init__ = __init__
        return cls
    return wrapper(args[0]) if args else wrapper


@dataclass
class Position:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0


@nested_dataclass
class InterferentConfig:
    position: Position = field(default_factory=Position)
    offTime: str = "ns3::ExponentialRandomVariable[Mean=0.25|Bound=10]"
    burstSize: str = "ns3::ExponentialRandomVariable[Mean=100|Bound=500]"
    rtsCtsThreshold: int = 4692480
    remoteStationManager: str = "ns3::MinstrelHtWifiManager"
    dataMode: str = "OfdmRate6Mbps"


@nested_dataclass
class StaConfig:
    position: Position = field(default_factory=Position)
    payloadSize: int = 50
    packetInterval: float = 0.5
    rtsCtsThreshold: int = 4692480
    remoteStationManager: str = "ns3::MinstrelHtWifiManager"
    dataMode: str = "OfdmRate6Mbps"

@nested_dataclass
class JsonConfig:
    sta: StaConfig = field(default_factory=StaConfig)
    interferent: List[InterferentConfig] = field(default_factory=list)
    simulationTime: float = 10
    enablePcap: bool = False
    pcapPrefix: str = ""

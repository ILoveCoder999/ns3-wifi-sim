from utils.common import nested_dataclass, Position
from dataclasses import field
from typing import Tuple, List

def get_pos_factory(coords: Tuple[float, float, float]):    
    def pos_factory(*args, **kwargs):
        return Position(x=coords[0], y= coords[1], z = coords[2])
    return pos_factory

@nested_dataclass
class Interferer:
    position: Position =  field(default_factory=get_pos_factory((120, 0, 0)))
    channel_idx: int = 0

@nested_dataclass
class HandoverConfig:
    simTime: float = 60000

    port: int = 9
    payloadSize: int = 22
    packetInterval: int = 0.03
    doubleChannel: bool = False
    constantRate: bool = False

    channels: List[str] = field(default_factory= lambda : ["{44,20,BAND_5GHZ,0}", "{40,20,BAND_5GHZ,0}"])
    interferers: List[Interferer] = field(default_factory= list)
    
    tripTime: float = 300
    repetitions: int = 100
    staPosStart: Position = field(default_factory=get_pos_factory((0, 0, 0)))
    staPosEnd: Position = field(default_factory=get_pos_factory((150, 0, 0)))

    apPositions: List[Position] = field(default_factory=lambda: [Position(50, 0, 0), Position(100, 0, 0)])

    enablePcap: bool = False
    enableAnimation: bool = False

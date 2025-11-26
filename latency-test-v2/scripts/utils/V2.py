from typing import List
from dataclasses import dataclass, field, asdict
import json

from utils.common import nested_dataclass, Position


@dataclass
class SpectrumChannelConfig:
    propagationLossModel: str
    propagationDelayModel: str


@nested_dataclass
class SpectrumPhyConfig:
    channelSettings: str
    channel: SpectrumChannelConfig


@nested_dataclass
class ApConfig:
    position: Position = field(default_factory=Position)
    ssid: str = "ssid_1"
    phyId: int = 0


@nested_dataclass
class InterfererConfig:
    position: Position = field(default_factory=Position)
    offTime: str = "ns3::ExponentialRandomVariable[Mean=0.25|Bound=10]"
    burstSize: str = "ns3::ExponentialRandomVariable[Mean=100|Bound=500]"
    rtsCtsThreshold: int = 4692480
    remoteStationManager: str = "ns3::MinstrelHtWifiManager"
    dataMode: str = "OfdmRate6Mbps"
    ssid: str = "ssid_1"
    phyId: int = 0


@nested_dataclass
class StaMobility:
    mobilityModel: str = "ns3::ConstantPositionMobilityModel"  #"ns3::WaypointMobilityModel"
    startPos: Position = field(default_factory=Position)
    endPos: Position = field(default_factory=Position)
    tripTime: float = 0
    timeOffset: float = 3
    repetitions: int = 1
    

@nested_dataclass
class StaConfig:
    position: Position = field(default_factory=Position)
    payloadSize: int = 50
    packetInterval: float = 0.5
    rtsCtsThreshold: int = 4692480
    remoteStationManager: str = "ns3::MinstrelHtWifiManager"
    dataMode: str = "OfdmRate6Mbps"
    ssid: str = "ssid_1"
    phyId: int = 0
    mobility: StaMobility =  field(default_factory=StaMobility)


@nested_dataclass
class JsonConfig:
    staNode: StaConfig = field(default_factory=StaConfig)
    phyConfigs: List[SpectrumPhyConfig] = field(default_factory=list)
    apNodes: List[ApConfig] = field(default_factory=list)
    interfererNodes: List[InterfererConfig] = field(default_factory=list)
    simulationTime: float = 10
    enablePcap: bool = False
    pcapPrefix: str = ""
    runNumber: int = 1


class JsonConfigEncoder(json.JSONEncoder):

    def default(self, o):
        if isinstance(o, (JsonConfig, StaConfig, InterfererConfig, ApConfig, SpectrumPhyConfig)):
            return asdict(o)
        return super().default(o)


@dataclass
class RetransmissionInfo:
    rate: int
    latency: int


@nested_dataclass
class PacketInfo:
    acked: bool
    latency: int
    retransmissions: List[RetransmissionInfo]
    seq: int

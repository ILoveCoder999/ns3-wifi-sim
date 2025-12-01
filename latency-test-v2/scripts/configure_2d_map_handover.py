from utils.V2 import JsonConfig, StaConfig, Position, InterfererConfig, ApConfig, SpectrumPhyConfig, SpectrumChannelConfig
import json
from dataclasses import asdict

configs = []

simulationTime = 3600


def float_range(start: float, stop: float, step: float, inclusive=False):
    val = start
    while (inclusive and val <= stop) or (not inclusive and val < stop):
        yield val
        val += step



simulations_2d = [
    ([
         ApConfig(
             position=Position(x=0, y=0),
             ssid="ssid_1",
             phyId=0
         )
     ],
     [
         InterfererConfig(
             position=Position(x=-20, y=-20),
             ssid="ssid_1",
             phyId=0
         ),
         InterfererConfig(
             position=Position(x=-20, y=20),
             ssid="ssid_1",
             phyId=0
         )
     ]),
    ([
         ApConfig(
             position=Position(x=0, y=0),
             ssid="ssid_1",
             phyId=0
         )
     ],
     [
         InterfererConfig(
             position=Position(x=-20, y=-30),
             ssid="ssid_1",
             phyId=0
         ),
         InterfererConfig(
             position=Position(x=-20, y=30),
             ssid="ssid_1",
             phyId=0
         )
     ])
]

for apNodes, interfererNodes in simulations_2d:
    for staX in float_range(-50, 50, 1, inclusive=True):
        for staY in float_range(-50, 50, 1, inclusive=True):
            configs.append(JsonConfig(
                phyConfigs=[
                    SpectrumPhyConfig(
                        channelSettings="{44,20,BAND_5GHZ,0}",
                        channel=SpectrumChannelConfig(
                            propagationLossModel="ns3::LogDistancePropagationLossModel",
                            propagationDelayModel="ns3::ConstantSpeedPropagationDelayModel"
                        )
                    )
                ],
                staNode=StaConfig(
                    position=Position(x=staX, y=staY),
                    payloadSize=22,  # Per avere il pacchetto della stessa dimensione del caso reale
                    ssid="ssid_1",
                    phyId=0
                ),
                apNodes=apNodes,
                interfererNodes=interfererNodes,
                simulationTime=simulationTime
            ))

print(len(configs), "simulations")
with open("./handover_map_conf.json", "w") as f:
    json.dump([asdict(c) for c in configs], f, indent=2)

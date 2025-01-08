from utils.V2 import JsonConfig, StaConfig, Position, InterfererConfig, ApConfig, SpectrumPhyConfig, SpectrumChannelConfig
import json
from dataclasses import asdict

configs = []

simulationTime = 30000


def float_range(start: float, stop: float, step: float, inclusive=False):
    val = start
    while (inclusive and val <= stop) or (not inclusive and val < stop):
        yield val
        val += step



simulations_1d = [
    # Config without interferents
    ([
        ApConfig(
            position=Position(x=0, y=0),
            ssid="ssid_1",
            phyId=0
        )
     ], []), 
    # Traslating interferent positions
    ([
        ApConfig(
            position=Position(x=0, y=0),
            ssid="ssid_1",
            phyId=0
        )
     ], [
        InterfererConfig(
            position=Position(x=-40, y=2),
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
     ], [
        InterfererConfig(
            position=Position(x=-20, y=2),
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
     ], [
        InterfererConfig(
            position=Position(x=0, y=2),
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
     ], [
        InterfererConfig(
            position=Position(x=20, y=2),
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
     ], [
        InterfererConfig(
            position=Position(x=40, y=2),
            ssid="ssid_1",
            phyId=0
        )
    ]),
    ## Distant interferent connected to a second access point
    ([
        ApConfig(
            position=Position(x=0, y=0),
            ssid="ssid_1",
            phyId=0
        ),
        ApConfig(
            position=Position(x=100, y=0),
            ssid="ssid_2",
            phyId=0
        )
     ], [
        InterfererConfig(
            position=Position(x=60, y=2),
            ssid="ssid_2",
            phyId=0
        )
    ]),
    ([
        ApConfig(
            position=Position(x=0, y=0),
            ssid="ssid_1",
            phyId=0
        ),
        ApConfig(
            position=Position(x=100, y=0),
            ssid="ssid_2",
            phyId=0
        )
     ], [
        InterfererConfig(
            position=Position(x=80, y=2),
            ssid="ssid_2",
            phyId=0
        )
    ]),
    ## Two interferents at same pos
    ([
        ApConfig(
            position=Position(x=0, y=0),
            ssid="ssid_1",
            phyId=0
        )
     ], [
        InterfererConfig(
            position=Position(x=-40, y=2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=-2),
            ssid="ssid_1",
            phyId=0
        )
    ]),
    # Three interferents at same pos
    ([
        ApConfig(
            position=Position(x=0, y=0),
            ssid="ssid_1",
            phyId=0
        )
     ], [
        InterfererConfig(
            position=Position(x=-40, y=2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=-2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=4),
            ssid="ssid_1",
            phyId=0
        )
    ]),
    # Four interferents at same pos
    ([
        ApConfig(
            position=Position(x=0, y=0),
            ssid="ssid_1",
            phyId=0
        )
     ], [
        InterfererConfig(
            position=Position(x=-40, y=2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=-2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=4),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=-4),
            ssid="ssid_1",
            phyId=0
        )
    ]),
    # Five interferents at same pos
    ([
        ApConfig(
            position=Position(x=0, y=0),
            ssid="ssid_1",
            phyId=0
        )
     ], [
        InterfererConfig(
            position=Position(x=-40, y=2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=-2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=4),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=-4),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-40, y=6),
            ssid="ssid_1",
            phyId=0
        )
    ])
]

for apNodes, interfererNodes in simulations_1d:
    for staX in range(0, 51, 1):
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
                position=Position(x=staX, y=0),
                payloadSize=22,  # Per avere il pacchetto della stessa dimensione del caso reale
                ssid="ssid_1",
                phyId=0
            ),
            apNodes=apNodes,
            interfererNodes=interfererNodes,
            simulationTime=simulationTime
        ))

print("{} configurations {} simulations".format(len(simulations_1d), len(configs)))
with open("./wfcs_simulations_01.json", "w") as f:
    json.dump([asdict(c) for c in configs], f, indent=2)

from utils.V2 import JsonConfig, StaConfig, Position, InterfererConfig, ApConfig, SpectrumPhyConfig, SpectrumChannelConfig
import json
from dataclasses import asdict

def float_range(start: float, stop: float, step: float, inclusive=False):
    val = start
    while (inclusive and val <= stop) or (not inclusive and val < stop):
        yield val
        val += step

SIMULATIONS_2D = [
    ([
        ApConfig(
            position=Position(x=0, y=0),
            ssid="ssid_1",
            phyId=0
        )
     ], []),
    ([
         ApConfig(
             position=Position(x=0, y=0),
             ssid="ssid_1",
             phyId=0
         )
     ],
     [
        InterfererConfig(
            position=Position(x=-30, y=2),
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
            position=Position(x=-30, y=2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-30, y=-2),
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
            position=Position(x=-30, y=2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-30, y=-2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-30, y=4),
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
            position=Position(x=-30, y=2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-30, y=-2),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-30, y=4),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-30, y=-4),
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
            position=Position(x=-15, y=-15),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-15, y=-30),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-30, y=-15),
            ssid="ssid_1",
            phyId=0
        ),
        InterfererConfig(
            position=Position(x=-30, y=-30),
            ssid="ssid_1",
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
             position=Position(x=-15, y=-15),
             ssid="ssid_2",
             phyId=0
         ),
         ApConfig(
             position=Position(x=-15, y=15),
             ssid="ssid_3",
             phyId=0
         )
     ], []),
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
     ]),
    ([
         ApConfig(
             position=Position(x=0, y=0),
             ssid="ssid_1",
             phyId=0
         ),
         ApConfig(
             position=Position(x=-30, y=-30),
             ssid="ssid_2",
             phyId=0
         ),
         ApConfig(
             position=Position(x=-30, y=30),
             ssid="ssid_3",
             phyId=0
         )
     ], []),
    ([
         ApConfig(
             position=Position(x=0, y=0),
             ssid="ssid_1",
             phyId=0
         )
     ],
     [
         InterfererConfig(
             position=Position(x=-30, y=30),
             ssid="ssid_1",
             phyId=0
         ),
         InterfererConfig(
             position=Position(x=30, y=-30),
             ssid="ssid_1",
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
            position=Position(x=-30, y=30),
            ssid="ssid_2",
            phyId=0
        ),
        ApConfig(
            position=Position(x=30, y=30),
            ssid="ssid_3",
            phyId=0
        )
     ],
     [
         InterfererConfig(
             position=Position(x=-10, y=10)
         ),
         InterfererConfig(
             position=Position(x=10, y=0)
         ),
         InterfererConfig(
             position=Position(x=-10, y=-10)
         ),
         InterfererConfig(
             position=Position(x=10, y=-20)
         ),
         InterfererConfig(
             position=Position(x=50, y=30),
             ssid="ssid_3",
             phyId=0
         ),
         InterfererConfig(
             position=Position(x=70, y=20),
             ssid="ssid_3",
             phyId=0
         )
    ])
]


SIMULATION_TIME = 30000
CONFIG_NUM = 0
STA_POS = (2.5, 2.5)

def main():
    apNodes, interfererNodes = SIMULATIONS_2D[CONFIG_NUM]

    single_config = JsonConfig(
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
            position=Position(x=STA_POS[0], y=STA_POS[1]),
            payloadSize=22,  # Per avere il pacchetto della stessa dimensione del caso reale
            ssid="ssid_1",
            phyId=0
        ),
        apNodes=apNodes,
        interfererNodes=interfererNodes,
        simulationTime=SIMULATION_TIME
    )

    with open("./single_sim_conf.json", "w") as f:
        json.dump(asdict(single_config), f, indent=2)

if __name__ == "__main__":
    main()

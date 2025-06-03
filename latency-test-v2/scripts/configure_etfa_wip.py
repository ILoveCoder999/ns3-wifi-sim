from utils.V2 import JsonConfig, StaConfig, Position, InterfererConfig, ApConfig, SpectrumPhyConfig, SpectrumChannelConfig, StaMobility
import json
from dataclasses import asdict
import math

data_rates = [
    "OfdmRate6Mbps",
    "OfdmRate9Mbps",
    "OfdmRate12Mbps",
    "OfdmRate18Mbps",
    "OfdmRate24Mbps",
    "OfdmRate36Mbps",
    "OfdmRate48Mbps",
    "OfdmRate54Mbps"
]

sta_speeds = [
    1,
    0.1,
    0.01
]

net_configs = [
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
            position=Position(x=0, y=2),
            ssid="ssid_1",
            phyId=0
        )
    ]),   
]

def generate_constant_rate(net_list, data_rates, sim_time, fname):
    configs = []
    for apNodes, interfererNodes in net_list:
        for data_rate in data_rates:
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
                        phyId=0,
                        remoteStationManager = "ns3::ConstantRateWifiManager",
                        dataMode= data_rate
                    ),
                    apNodes=apNodes,
                    interfererNodes=interfererNodes,
                    simulationTime=sim_time
                ))
    print("{} configurations {} simulations".format(len(net_list), len(configs)))
    with open(fname, "w") as f:
        json.dump([asdict(c) for c in configs], f, indent=2)

def generate_moving_sta(net_list, sta_speeds, sim_time, fname):
    configs = []
    for apNodes, interfererNodes in net_list:
            for sta_speed in sta_speeds:
                time_offset = 2.25
                trip_time = round(51 / sta_speed)
                repetitions = round(sim_time/trip_time * 50)
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
                        payloadSize=22,  # Per avere il pacchetto della stessa dimensione del caso reale
                        ssid="ssid_1",
                        phyId=0,
                        mobility = StaMobility(
                            mobilityModel = "ns3::WaypointMobilityModel",
                            startPos = Position(x=0, y=0),
                            endPos = Position(x=51, y=0),
                            tripTime = trip_time,
                            timeOffset = time_offset,
                            repetitions = repetitions
                        )
                    ),
                    apNodes=apNodes,
                    interfererNodes=interfererNodes,
                    simulationTime= round(trip_time * repetitions + time_offset)
                ))
    print("{} configurations {} simulations".format(len(net_list), len(configs)))
    with open(fname, "w") as f:
        json.dump([asdict(c) for c in configs], f, indent=2)

def main():
    generate_constant_rate(net_list=net_configs, data_rates=data_rates, sim_time=30000, fname="./etfa_wip_constant_rate.json")
    generate_moving_sta(net_list=net_configs, sta_speeds=sta_speeds, sim_time=30000, fname="./etfa_wip_moving_sta.json")

if __name__ == "__main__":
    main()

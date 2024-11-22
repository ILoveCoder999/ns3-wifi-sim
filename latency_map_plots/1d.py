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
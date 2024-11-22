for apNodes, interfererNodes in simulations_2d:
    for staX in float_range(-50, 50, 2.5, inclusive=True):
        for staY in float_range(-50, 50, 2.5, inclusive=True):
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
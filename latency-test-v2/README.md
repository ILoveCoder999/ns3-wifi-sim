# Code information
This file contains notes about the code in this folder

## latency-test-v2 (for latency maps)

NS3 WiFi documentation
https://www.nsnam.org/docs/models/html/wifi-design.html
https://www.nsnam.org/docs/models/html/wifi-user.html


PHYSICAL SETTINGS (https://www.nsnam.org/docs/models/html/wifi-design.html#phy-layer-models):
- "ns3::LogDistancePropagationLossModel" (reception power)
- "ns3::ConstantSpeedPropagationDelayModel" (costant propation in medium)
- "{44,20,BAND_5GHZ,0}" ({CHANNEL_NUM, CHANNEL_WIDTH_MHZ, FREQ_BAND, Che sottocanale da 20 è il primario)

WIFI STANDARD:
- WIFI_STANDARD_80211a (vecchio per disabilitare tutte le features, e.g. frame aggregation)

HIGH-MAC MODELS
- ns3::WifiMac
    - Access Point (AP) (ns3::ApWifiMac)
        - generates periodic beacons
        - accepts every attempt to associate
    - non-AP Station (STA) (ns3::StaWifiMac)
        - active probing
        - automatic re-association if beacons are missed
    - STA in an Independent Basic Service Set (IBSS) aka "ad hoc network" (ns3::AdhocWifiMac)   
        - no probing, beacon, or association
- Rate control algorithms (remoteStationManager):
    - types
        - ns3::ConstantRateWifiManager
        - ns3::MinstrelHtWifiManager (X)
    - attributes
        - MaxSsrc = 21 (maximun number of transmission attempts of packets with size below RtsCtsThreshold)
        - RtsCtsThreshold = 4692480 (threshold to packet size before using RtsCts)
        - DataMode (WifiMode) = OfdmRate6Mbps (ONLY for constant rate wifi)

MOBILITY
- ns3::ConstantPositionMobilityModel

STA application (my-udp-client, TAKEN FROM THE INTERNET):
- MaxPackets (maximum number of packets, zero is infinite)
- Interval (time in-between packets in seconds)
    - 0.5
- IntervalJitter (variation of interval in seconds)
    - ns3::UniformRandomVariable[Min=-0.000025|Max=0.000075]
- PacketSize
    - 22

Interferent Application (my-udp-client, written by Matteo):
- PeerAddress (address of access point)
- OffTime (time between burst in seconds)
    - ns3::ExponentialRandomVariable[Mean=0.25|Bound=10]
- BurstSize (number of packets in burst)
    - ns3::ExponentialRandomVariable[Mean=100|Bound=500]
- BurstPacketsInterval
    - 500 microseconds (default in arg structure)
- BurstPacketsSize
    - 1500 (size of packets in the burst)


## handover
# ns-3 simulations

## ns-3 documentation
- [Object model/Aggregation](https://www.nsnam.org/docs/manual/html/object-model.html)
- [Wifi module - User documentation](https://www.nsnam.org/docs/models/html/wifi-user.html)
- [Wifi module - Design documentation](https://www.nsnam.org/docs/models/html/wifi-design.html)
- [Mobility module](https://www.nsnam.org/docs/models/html/mobility.html#mobility)
- [Building topologies (Tutorial)](https://www.nsnam.org/docs/release/3.44/tutorial/html/building-topologies.html#building-a-wireless-network-topology)
- [Random Variables](https://www.nsnam.org/docs/manual/html/random-variables.html#seeding-and-independent-replications)

## Folder structure
This is the general structure of the folder.
```bash
.
├── old_matteo      # old files
├── scripts         # python scripts to generate sim configurations
├── simulations     # cache folder for sim configurations / sim outputs
├── *.cc
├── *.h
├── CMakeLists.txt
└── README.md
```

## Simulation programs

Simulation program source files are the following:

```bash
├── handover.cc             
├── latency-test-v2.cc
└── reassociation_test.cc
```

### reassociation_test.cc
This program is used to test if channel switching works propertly.

Network composition:
- 1 SUT (STA under test)
- 2 APs
- 1 UDP server node.

The SUT is fixed at a location in the rage of two APs, working on separate channels.
The two APs the UPD server node (to which the SUT sends packets) are connected together with a bridge (csma network).
A periodic timer is set to make the APs switch the operating channel, forcing a disconnection and re-connection.

The program **DOES NOT** accept json configiguration files as input, and its behaviour can be modified by changing the values in the ```HandoverConfig``` struct defined inside the source file.

### handover.cc
This program simulates the handover of a SUT travelling back and forth between two APs.

Network composition:
- 1 SUT (STA under test)
- 2 APs
- 1 UDP server node.
- (Optional) interfererings STAs.

The two APs the UPD server node (to which all STAs send packets) are connected together with a bridge (csma network).
The disconnections is triggered after the SUT loses three beacons from the AP to which it is connected (ns-3 default behavior).
If the ```doubleChannel``` is set to true, the APs are configured to work on separated channels, and, when the SUT disconnect from on of the AP, it switches to the channel of the other.

This program accepts as a paramter json configuration files (either inline or via file name) structured as defined by the ```HandoverConfig``` struct in its source file.
If a json configuration files is not provides, the default values defined in the struct are used.

### latency-test-v2.cc
This program simulates the traffic exchange between a fixed or moving SUT connected to a single AP.

Network composition:
- 1 SUT (STA under test)
- 1 AP
- (Optional) interferering STAs.
- (Optional) interferering APs.

This program **REQUIRES** as a parameter a json configuration files (either inline or via file name) structured as defined by the ```Arguments``` struct defined in the ```arguments.h``` file.


## Generate and run sim configurations




## Network parameters (to check)
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

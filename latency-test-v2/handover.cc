#include "interferer-application-helper.h"
#include "packet-info.h"
#include "utils.h"
#include "sta-logger.h"
#include "my-udp-client-helper.h"


#include <iostream>
#include <fstream>
#include <sstream>

#include "ns3/application-container.h"
#include "ns3/arp-cache.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/llc-snap-header.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/object-vector.h"
#include "ns3/pointer.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/seq-ts-header.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-header.h"
#include "ns3/waypoint-mobility-model.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-mpdu.h"
#include "ns3/wifi-net-device.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-helper.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <unordered_map>

#include "ns3/netanim-module.h"
#include "ns3/timer.h"

using json = nlohmann::json;

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("handover");

bool ENABLE_PCAP = true;
bool ANIMATION = false;
double SIM_TIME = 300;
uint32_t PORT = 9;
uint32_t PAYLOAD_SIZE = 22;
double PACKET_INTERVAL = 0.5;

int pos_counter = 0;
void courseChangeCallback(Ptr< const MobilityModel > model) {
    NS_LOG_FUNCTION(pos_counter);
    pos_counter+=1;
}

void timerCallback(Timer* timer, Ptr<WaypointMobilityModel> staMobility) {
    NS_LOG_FUNCTION(Simulator::Now().GetSeconds());
    NS_LOG_FUNCTION(staMobility->GetPosition());
    timer->Schedule(Seconds(1));
}

int main(int argc, char** argv) {

    LogComponentEnable("handover", LOG_LEVEL_DEBUG);

    //constexpr uint32_t port = 9;
    std::string outFilePath = "db0.json";
    std::string jsonConfig = "conf.json";
    //bool inlineConfig = false;

    // Set seed
    RngSeedManager::SetSeed(1);
    // Packet::EnablePrinting();

    // Create nodes
    NodeContainer wifiApNodes;
    //wifiApNodes.Create(2);
    wifiApNodes.Create(1);
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

    // Create PHY helper
    SpectrumWifiPhyHelper spectrumPhyHelper;

    Ptr<SpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<PropagationLossModel> propagationLossModel = CreateObject<LogDistancePropagationLossModel>();
    Ptr<PropagationDelayModel> propagationDelayModel = CreateObject<ConstantSpeedPropagationDelayModel>();

    spectrumChannel->AddPropagationLossModel(propagationLossModel);
    spectrumChannel->SetPropagationDelayModel(propagationDelayModel);

    spectrumPhyHelper.SetChannel(spectrumChannel);
    spectrumPhyHelper.Set("ChannelSettings", StringValue("{44,20,BAND_5GHZ,0}"));

    // Creater Wi-Fi helper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager",
        "MaxSsrc", UintegerValue(21),
        "RtsCtsThreshold", UintegerValue(4692480));
    // wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
    //     "MaxSsrc", UintegerValue(21),
    //     "RtsCtsThreshold", UintegerValue(4692480),
    //     "DataMode", StringValue("OfdmRate6Mbps"));

    // Create mac helper. instal Wifi on nodes to create net devices
    NetDeviceContainer staDevice;
    NetDeviceContainer apDevices;

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ssid_1")));
    staDevice = wifi.Install(spectrumPhyHelper, wifiMac, wifiStaNode);

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("ssid_1")));
    apDevices.Add(wifi.Install(spectrumPhyHelper, wifiMac, wifiApNodes.Get(0)));
    //apDevices.Add(wifi.Install(spectrumPhyHelper, wifiMac, wifiApNodes.Get(1)));

    // Create vector of positions for the two APs
    Ptr<ListPositionAllocator> positionAllocator = CreateObject<ListPositionAllocator>();
    positionAllocator->Add(Vector(50, 0, 0));
    //positionAllocator->Add(Vector(100, 0, 0));

    // Configure and install mobility for APs
    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    // Configure and install mobility model for STA
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(wifiStaNode);
    Ptr<WaypointMobilityModel> staMobilityModel = DynamicCast<WaypointMobilityModel>(wifiStaNode.Get(0)->GetObject<MobilityModel>());
    staMobilityModel->AddWaypoint(Waypoint(Seconds(0),Vector(0, 0, 0)));
    staMobilityModel->AddWaypoint(Waypoint(Seconds(300), Vector(150, 0, 0)));
    //staMobilityModel->AddWaypoint(Waypoint(Seconds(600), Vector(0, 0, 0)));

    std::stringstream ss;
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/$ns3::MobilityModel/$ns3::WaypointMobilityModel/CourseChange";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&courseChangeCallback));

    // // Adding LAN nodes
    NodeContainer csmaNodes;
    csmaNodes.Create(1);
    csmaNodes.Add(wifiApNodes);

    // Configure CSMA
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    csma.SetDeviceAttribute("EncapsulationMode", StringValue("Llc"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1492)); // to be in spec when using LLC (size not more than 1518)
    
    // Create CSMA devices
    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    //Configure bridge
    BridgeHelper brh;
    NetDeviceContainer bridgeDevices, toBridgeDevices;
    toBridgeDevices.Add(apDevices.Get(0));
    toBridgeDevices.Add(csmaDevices.Get(1));
    bridgeDevices = brh.Install(wifiApNodes.Get(0), toBridgeDevices);

    // Install internet stack
    InternetStackHelper internetStack;
    // stack.SetRoutingHelper();
    internetStack.Install(wifiApNodes);
    internetStack.Install(wifiStaNode);
    internetStack.Install(csmaNodes.Get(0));

    // Create Wi-Fi and LAN subnets
    Ipv4AddressHelper address;

    Ipv4InterfaceContainer apInterfaces, staInterface;
    address.SetBase("192.168.1.0", "255.255.255.0");
    staInterface = address.Assign(staDevice);    
    apInterfaces = address.Assign(apDevices);    

    Ipv4InterfaceContainer csmaInterfaces;
    //address.SetBase("192.168.2.0", "255.255.255.0");    
    csmaInterfaces = address.Assign(csmaDevices);

    // Install applications on nodes
    ApplicationContainer clientApp;
    ApplicationContainer serverApp;

    // UDP server on APs (set start time)
    UdpServerHelper server(PORT);
    serverApp = server.Install(csmaNodes.Get(0));
    serverApp.Start(Seconds(1));
    serverApp.Stop(Seconds(SIM_TIME + 1));

    // Custom UDP client on STA
    MyUdpClientHelper client(csmaInterfaces.GetAddress(0), PORT);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(PACKET_INTERVAL)));
    client.SetAttribute("IntervalJitter", StringValue("ns3::UniformRandomVariable[Min=-0.000025|Max=0.000075]"));
    client.SetAttribute("PacketSize", UintegerValue(PAYLOAD_SIZE));
    clientApp = client.Install(wifiStaNode);
    clientApp.Start(Seconds(1));
    clientApp.Stop(Seconds(SIM_TIME + 1));

    // Enable pcap on all nodes (physical layer)
    if (ENABLE_PCAP)
    {
        spectrumPhyHelper.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        spectrumPhyHelper.EnablePcap("handover-sta", staDevice);
        spectrumPhyHelper.EnablePcap("handover-ap", apDevices.Get(0));
        //spectrumPhyHelper.EnablePcap("handover-ap", apDevices.Get(1));

        csma.EnablePcapAll("handover-csma", true);
    }

    // Populate Arp Cache and routing tables
    // PopulateArpCache();
    // Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Start simulation
    Simulator::Stop(Seconds(SIM_TIME));

    if(ANIMATION) {
        AnimationInterface anim ("handover_anim.xml");
    }

    Timer timer = Timer();
    timer.SetFunction(&timerCallback);
    timer.SetArguments(&timer, staMobilityModel);
    timer.Schedule(Seconds(1));

    //Simulator::Schedule(Seconds(0.1), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

#include "interferer-application-helper.h"
#include "packet-info.h"
#include "utils.h"
#include "sta-logger.h"
#include "my-udp-client-helper.h"
#include "assoc-logger.h"
#include "roaming-manager.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <unordered_map>

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

#include "ns3/netanim-module.h"
#include "ns3/timer.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("handover");

struct OutputConfig
{
    std::string STA_LOG_PATH = "handover_sta_log.json";
    std::string ASSOC_LOG_PATH = "handover_assoc_log.json";
};

struct Position
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct Interferer {
    Position position = {120, 0, 0};
    std::size_t channel_idx = 0;
};

struct HandoverConfig {
    double simTime = 60000;
    
    uint32_t port = 9;
    uint32_t payloadSize = 22;
    double packetInterval = 0.03;
    bool doubleChannel = false;
    bool constantRate = false;

    std::vector<std::string> channels = {"{44,20,BAND_5GHZ,0}", "{40,20,BAND_5GHZ,0}"};
    std::vector<Interferer> interferers = {};//{Interferer{}};

    double tripTime = 300;
    uint32_t repetitions = 100;
    Position staPosStart = {0, 0, 0};
    Position staPosEnd = {150, 0, 0};

    std::vector<Position> apPositions = {Position{60, 0, 0}, Position{90, 0, 0}};

    bool enablePcap = false;
    bool enableAnimation = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Position, x, y, z);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Interferer, position, channel_idx);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    HandoverConfig, 
    simTime,
    port, payloadSize, packetInterval, doubleChannel, constantRate,
    channels, interferers,
    tripTime, repetitions, staPosStart, staPosEnd,
    apPositions,
    enablePcap, enableAnimation
);

inline std::ostream& operator<<(std::ostream& stream, const HandoverConfig& conf)
{
  return stream << json(conf);
}

inline std::istream& operator>>(std::istream& stream, HandoverConfig& conf)
{
  json j;
  stream >> j;
  conf = j.get<HandoverConfig>();
  return stream;
}

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
    //LogComponentEnable("handover", LOG_LEVEL_DEBUG);
    LogComponentEnable("RoamingManager", LOG_LEVEL_DEBUG);

    HandoverConfig sim_config;    
    OutputConfig out_config;

    // Parse line arguments (if used)
    if (argc > 1)
    {
        std::string jsonConfig = "conf.json";
        bool inlineConfig = false;
        CommandLine cmd(__FILE__);
        cmd.AddValue("jsonConfig", "Json configuration", jsonConfig);
        cmd.AddValue("staLogFile", "STA log file path", out_config.STA_LOG_PATH);
        cmd.AddValue("assocLogFile", "Association log file path", out_config.ASSOC_LOG_PATH);
        cmd.AddValue("inlineConfig", "Provide config inline", inlineConfig);
        cmd.Parse(argc, argv);
        std::cout << jsonConfig << " " << out_config.STA_LOG_PATH << " " << out_config.ASSOC_LOG_PATH << " " << inlineConfig << std::endl;
        if (inlineConfig)
        {
            std::stringstream conf_stream(jsonConfig);
            conf_stream >> sim_config;
        }
        else {
            std::ifstream arg_file;  
            arg_file.open(jsonConfig.c_str(), std::ios::in);
            if(!arg_file)
            {
                return 1;
            }
            arg_file >> sim_config;
        }
    }

    // Set seed
    RngSeedManager::SetSeed(1);
    // Packet::EnablePrinting();

    // Create Wi-Fi nodes
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

    // // Create Wi-Fi interferer nodes
    NodeContainer wifiInterfererNodes;
    wifiInterfererNodes.Create(sim_config.interferers.size());

    // Adding LAN nodes
    NodeContainer csmaNodes;
    csmaNodes.Add(wifiApNodes); // AP also have csma interface
    csmaNodes.Create(1);

    // Create PHY helper
    SpectrumWifiPhyHelper spectrumPhyHelper;
    Ptr<SpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<PropagationLossModel> propagationLossModel = CreateObject<LogDistancePropagationLossModel>();
    Ptr<PropagationDelayModel> propagationDelayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    spectrumChannel->AddPropagationLossModel(propagationLossModel);
    spectrumChannel->SetPropagationDelayModel(propagationDelayModel);
    spectrumPhyHelper.SetChannel(spectrumChannel);
    spectrumPhyHelper.Set("ChannelSettings", StringValue(sim_config.channels.at(0)));

    // Creater Wi-Fi helper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    if (!sim_config.constantRate) {
        wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager",
            "MaxSsrc", UintegerValue(21),
            "RtsCtsThreshold", UintegerValue(4692480));
    }
    else {
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "MaxSsrc", UintegerValue(21),
            "RtsCtsThreshold", UintegerValue(4692480),
            "DataMode", StringValue("OfdmRate24Mbps"));
    }

    // Create mac helper. instal Wifi on nodes to create net devices
    NetDeviceContainer staDevice;
    NetDeviceContainer apDevices;
    NetDeviceContainer interfererDevices;

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ssid_1")));
    staDevice = wifi.Install(spectrumPhyHelper, wifiMac, wifiStaNode);

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("ssid_1")));
    apDevices.Add(wifi.Install(spectrumPhyHelper, wifiMac, wifiApNodes.Get(0)));
    if (sim_config.doubleChannel) {
        spectrumPhyHelper.Set("ChannelSettings", StringValue(sim_config.channels.at(1)));
    }
    apDevices.Add(wifi.Install(spectrumPhyHelper, wifiMac, wifiApNodes.Get(1)));

    // Configure and install STA Wifi on interferers
    for (std::size_t i = 0; i < sim_config.interferers.size(); i++)
    {
        const auto& wifiInterferentNode = wifiInterfererNodes.Get(i);
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ssid_1")));
        spectrumPhyHelper.Set("ChannelSettings", StringValue(sim_config.channels.at(sim_config.interferers.at(i).channel_idx)));
        interfererDevices.Add(wifi.Install(spectrumPhyHelper, wifiMac, wifiInterferentNode));
    }

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
    NetDeviceContainer bridgeDevices, toBridgeDevicesAP1, toBridgeDevicesAP2;
    toBridgeDevicesAP1.Add(apDevices.Get(0));
    toBridgeDevicesAP1.Add(csmaDevices.Get(0));
    bridgeDevices = brh.Install(wifiApNodes.Get(0), toBridgeDevicesAP1);
    toBridgeDevicesAP2.Add(apDevices.Get(1));
    toBridgeDevicesAP2.Add(csmaDevices.Get(1));
    bridgeDevices.Add(brh.Install(wifiApNodes.Get(1), toBridgeDevicesAP2));

    // Install internet stack
    InternetStackHelper internetStack;
    internetStack.Install(wifiApNodes);
    internetStack.Install(wifiStaNode);
    internetStack.Install(wifiInterfererNodes);
    internetStack.Install(csmaNodes.Get(2));

    // Create Wi-Fi and LAN subnets
    Ipv4AddressHelper address;

    Ipv4InterfaceContainer apInterfaces, staInterface;
    address.SetBase("192.168.1.0", "255.255.255.0");
    staInterface = address.Assign(staDevice);
    apInterfaces = address.Assign(apDevices);

    Ipv4InterfaceContainer interfererInterfaces;
    interfererInterfaces = address.Assign(interfererDevices);

    Ipv4InterfaceContainer csmaInterfaces;
    //address.SetBase("192.168.2.0", "255.255.255.0");
    csmaInterfaces = address.Assign(csmaDevices);   

    // Install applications on nodes
    ApplicationContainer clientApp;
    ApplicationContainer serverApp;
    ApplicationContainer interfererApps;

    // UDP server on APs (set start time)
    UdpServerHelper server(sim_config.port);
    serverApp = server.Install(csmaNodes.Get(2));
    serverApp.Start(Seconds(1));
    serverApp.Stop(Seconds(sim_config.simTime + 1));

    // Custom UDP client on STA
    MyUdpClientHelper client(csmaInterfaces.GetAddress(2), sim_config.port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(sim_config.packetInterval)));
    client.SetAttribute("IntervalJitter", StringValue("ns3::UniformRandomVariable[Min=-0.000025|Max=0.000075]"));
    client.SetAttribute("PacketSize", UintegerValue(sim_config.payloadSize));
    clientApp = client.Install(wifiStaNode);
    clientApp.Start(Seconds(1));
    clientApp.Stop(Seconds(sim_config.simTime + 1));

    // Interferer applications
    InterfererApplicationHelper interfererHelper;
    for (std::size_t i = 0; i < sim_config.interferers.size(); i++)
    {
        interfererHelper.SetAttribute("PeerAddress", Ipv4AddressValue(csmaInterfaces.GetAddress(2)));
        interfererHelper.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.25|Bound=10]"));
        interfererHelper.SetAttribute("BurstSize", StringValue("ns3::ExponentialRandomVariable[Mean=100|Bound=500]"));
        interfererHelper.SetAttribute("BurstPacketsInterval", TimeValue(MicroSeconds(500)));
        interfererHelper.SetAttribute("BurstPacketsSize", UintegerValue(1400));
        interfererApps.Add(interfererHelper.Install(wifiInterfererNodes.Get(i)));
    }
    interfererApps.Start(Seconds(1.0));
    interfererApps.Stop(Seconds(sim_config.simTime + 1));

    // Create mobility helper
    MobilityHelper mobility;

    // Configure and install mobility model for APs
    Ptr<ListPositionAllocator> apPositionAllocator = CreateObject<ListPositionAllocator>();
    Position apPos = sim_config.apPositions.at(0);
    apPositionAllocator->Add(Vector(apPos.x, apPos.y, apPos.z));
    apPos =sim_config.apPositions.at(1);
    apPositionAllocator->Add(Vector(apPos.x, apPos.y, apPos.z));
    mobility.SetPositionAllocator(apPositionAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    // Configure and install mobility model for STA
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(wifiStaNode);
    Ptr<WaypointMobilityModel> staMobilityModel = DynamicCast<WaypointMobilityModel>(wifiStaNode.Get(0)->GetObject<MobilityModel>());
    staMobilityModel->AddWaypoint(Waypoint(Seconds(0), Vector(sim_config.staPosStart.x, sim_config.staPosStart.y, sim_config.staPosStart.z)));
    for (uint32_t i = 0; i < sim_config.repetitions; ++i) {
        staMobilityModel->AddWaypoint(Waypoint(Seconds(sim_config.tripTime * (i*2 + 1)), Vector(sim_config.staPosEnd.x, sim_config.staPosEnd.y, sim_config.staPosEnd.z)));
        staMobilityModel->AddWaypoint(Waypoint(Seconds(sim_config.tripTime * (i*2 + 2)), Vector(sim_config.staPosStart.x, sim_config.staPosStart.y, sim_config.staPosStart.z)));
    }

    // Configure and install mobility model for Interferers
    Ptr<ListPositionAllocator> intPositionAllocator = CreateObject<ListPositionAllocator>();
    for (std::size_t i = 0; i < sim_config.interferers.size(); i++) {
        const Position& pos = sim_config.interferers.at(i).position;
        intPositionAllocator->Add(Vector(pos.x, pos.y, pos.z));
    }
    mobility.SetPositionAllocator(intPositionAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiInterfererNodes);
    
    // Create STA logger
    std::stringstream ss;
    ss << json(sim_config);
    STALogger sta_logger(out_config.STA_LOG_PATH, ss.str(), DynamicCast<WifiNetDevice>(staDevice.Get(0)), staMobilityModel);
    sta_logger.logHeader();

    // Tracing for sent packets
    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxPsduBegin";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&STALogger::sendingMpduCallback,  &sta_logger));

    // Tracing for acked packets
    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/AckedMpdu";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&STALogger::ackedMpduCallback, &sta_logger));

    // Tracing for response timeout
    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/MpduResponseTimeout";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&STALogger::mpduTimeoutCallback, &sta_logger));

    // Tracing for dropped packets
    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/DroppedMpdu";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&STALogger::droppedMpduCallback, &sta_logger));

    // Assoclogger
    AssocLogger assoc_logger(out_config.ASSOC_LOG_PATH, "{\"header\": \"ADD PARAMETERS\"}",  staMobilityModel);
    assoc_logger.logHeader();
    
    // Tracing new association
    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&AssocLogger::assocCallback, &assoc_logger));

    // Tracing de-association
    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&AssocLogger::deAssocCallback, &assoc_logger));

    // // Tracing beacon arrival
    // ss.str(std::string());
    // ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BeaconArrival";
    // Config::ConnectWithoutContext(ss.str(), MakeCallback(&AssocLogger::beaconArrivalCallback, &assoc_logger));

    // Tracing new beacon info
    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/ReceivedBeaconInfo";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&AssocLogger::receivedBeaconInfoCallback, &assoc_logger));

    // Tracing for mobility (polling)
    Timer timer = Timer();
    timer.SetFunction(&timerCallback);
    timer.SetArguments(&timer, staMobilityModel);
    timer.Schedule(Seconds(1));

    // // Tracing for course changes
    // ss.str(std::string());
    // ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/$ns3::MobilityModel/$ns3::WaypointMobilityModel/CourseChange";
    // Config::ConnectWithoutContext(ss.str(), MakeCallback(&courseChangeCallback));
    
    // Channel change
    RoamingManager roaming_manager(DynamicCast<WifiNetDevice>(staDevice.Get(0)), staMobilityModel, sim_config.channels);
    if (sim_config.doubleChannel) {       
        ss.str(std::string());
        ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc";
        Config::ConnectWithoutContext(ss.str(), MakeCallback(&RoamingManager::deAssocCallback, &roaming_manager));
    }

    // Enable pcap on all nodes (physical layer)
    if (sim_config.enablePcap)
    {
        spectrumPhyHelper.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        spectrumPhyHelper.EnablePcap("handover-sta", staDevice);
        spectrumPhyHelper.EnablePcap("handover-ap", apDevices.Get(0));
        spectrumPhyHelper.EnablePcap("handover-ap", apDevices.Get(1));
        csma.EnablePcapAll("handover-csma", true);        
    }

    // Populate Arp Cache and routing tables
    PopulateArpCache();
    // Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Start simulation
    Simulator::Stop(Seconds(sim_config.simTime));

    if(sim_config.enableAnimation) {
        AnimationInterface anim ("handover_anim.xml");
    }

    auto start = std::chrono::high_resolution_clock::now();
    Simulator::Run();

    // Log duration of simulation
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);
    sta_logger.logFooter(duration);
    assoc_logger.logFooter();

    Simulator::Destroy();
    return 0;
}

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
#include "ns3/command-line.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/node-container.h"
#include "ns3/object-vector.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/waypoint-mobility-model.h"
#include "ns3/wifi-mac.h"
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
    // Scanning mode: true = active (Probe Req/Res), false = passive (Beacon only)
    bool activeScanning = true;
    // Channel config: false = same channel (AP1 ch1, AP2 ch1), true = different (AP1 ch1, AP2 ch5)
    bool differentChannels = false;

    double simTime = 150;   // Must be >= 2*tripTime for a full round trip

    std::vector<std::string> channels = {"{36,20,BAND_5GHZ,0}", "{40,20,BAND_5GHZ,0}"};
    std::vector<Interferer> interferers = {};

    double tripTime = 75;   // Time (s) for STA to travel one way (start → end)
    uint32_t repetitions = 1;
    Position staPosStart = {0, 0, 0};
    Position staPosEnd = {150, 0, 0};

    std::vector<Position> apPositions = {Position{60, 0, 0}, Position{90, 0, 0}};

    uint32_t port = 9;
    uint32_t payloadSize = 22;
    double packetInterval = 0.03;
    bool doubleChannel = false;
    bool constantRate = false;

    bool enablePcap = true;
    bool enableAnimation = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Position, x, y, z);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Interferer, position, channel_idx);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    HandoverConfig,
    activeScanning, differentChannels,
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
void courseChangeCallback(Ptr<const MobilityModel> model) {
    NS_LOG_FUNCTION(pos_counter);
    pos_counter += 1;
}

void timerCallback(Timer* timer, Ptr<WaypointMobilityModel> staMobility) {
    NS_LOG_FUNCTION(Simulator::Now().GetSeconds());
    NS_LOG_FUNCTION(staMobility->GetPosition());
    timer->Schedule(Seconds(1));
}

int main(int argc, char** argv) {
    LogComponentEnable("RoamingManager", LOG_LEVEL_DEBUG);

    HandoverConfig sim_config;
    OutputConfig out_config;

    // Parse command-line arguments
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
            if (!arg_file)
            {
                return 1;
            }
            arg_file >> sim_config;
        }
    }

    RngSeedManager::SetSeed(1);

    // --- Nodes ---
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

    NodeContainer wifiInterfererNodes;
    wifiInterfererNodes.Create(sim_config.interferers.size());

    NodeContainer csmaNodes;
    csmaNodes.Add(wifiApNodes);
    csmaNodes.Create(1);

    // --- PHY / Channel ---
    SpectrumWifiPhyHelper spectrumPhyHelper;
    Ptr<SpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<PropagationLossModel> propagationLossModel = CreateObject<LogDistancePropagationLossModel>();
    Ptr<PropagationDelayModel> propagationDelayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    spectrumChannel->AddPropagationLossModel(propagationLossModel);
    spectrumChannel->SetPropagationDelayModel(propagationDelayModel);
    spectrumPhyHelper.SetChannel(spectrumChannel);
    spectrumPhyHelper.Set("ChannelSettings", StringValue(sim_config.channels.at(0)));

    // --- Wi-Fi ---
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    if (!sim_config.constantRate) {
        wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager",
            "MaxSsrc", UintegerValue(21),
            "RtsCtsThreshold", UintegerValue(4692480));
    } else {
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "MaxSsrc", UintegerValue(21),
            "RtsCtsThreshold", UintegerValue(4692480),
            "DataMode", StringValue("OfdmRate24Mbps"));
    }

    // --- MAC + devices ---
    NetDeviceContainer staDevice;
    NetDeviceContainer apDevices;
    NetDeviceContainer interfererDevices;

    WifiMacHelper wifiMac;

    // STA: active scanning sends Probe Req/Res; passive relies on Beacons
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(Ssid("ssid_1")),
                    "ActiveProbing", BooleanValue(sim_config.activeScanning));
    staDevice = wifi.Install(spectrumPhyHelper, wifiMac, wifiStaNode);

    // AP1 — always on channels[0]
    spectrumPhyHelper.Set("ChannelSettings", StringValue(sim_config.channels.at(0)));
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("ssid_1")));
    apDevices.Add(wifi.Install(spectrumPhyHelper, wifiMac, wifiApNodes.Get(0)));

    // AP2 — same channel as AP1 OR different channel depending on config
    if (sim_config.differentChannels) {
        spectrumPhyHelper.Set("ChannelSettings", StringValue(sim_config.channels.at(1)));
    }
    // (if same channel, ChannelSettings already set to channels[0])
    apDevices.Add(wifi.Install(spectrumPhyHelper, wifiMac, wifiApNodes.Get(1)));

    // Interferers
    for (std::size_t i = 0; i < sim_config.interferers.size(); i++)
    {
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ssid_1")));
        spectrumPhyHelper.Set("ChannelSettings", StringValue(sim_config.channels.at(sim_config.interferers.at(i).channel_idx)));
        interfererDevices.Add(wifi.Install(spectrumPhyHelper, wifiMac, wifiInterfererNodes.Get(i)));
    }

    // --- CSMA ---
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    csma.SetDeviceAttribute("EncapsulationMode", StringValue("Llc"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1492));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // --- Bridge (AP <-> CSMA) ---
    BridgeHelper brh;
    NetDeviceContainer bridgeDevices, toBridgeDevicesAP1, toBridgeDevicesAP2;
    toBridgeDevicesAP1.Add(apDevices.Get(0));
    toBridgeDevicesAP1.Add(csmaDevices.Get(0));
    bridgeDevices = brh.Install(wifiApNodes.Get(0), toBridgeDevicesAP1);
    toBridgeDevicesAP2.Add(apDevices.Get(1));
    toBridgeDevicesAP2.Add(csmaDevices.Get(1));
    bridgeDevices.Add(brh.Install(wifiApNodes.Get(1), toBridgeDevicesAP2));

    // --- Internet stack ---
    InternetStackHelper internetStack;
    internetStack.Install(wifiApNodes);
    internetStack.Install(wifiStaNode);
    internetStack.Install(wifiInterfererNodes);
    internetStack.Install(csmaNodes.Get(2));

    // --- IP addresses ---
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface  = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterfaces  = address.Assign(apDevices);
    Ipv4InterfaceContainer interfererInterfaces = address.Assign(interfererDevices);
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    // --- Applications ---
    ApplicationContainer serverApp, clientApp, interfererApps;

    UdpServerHelper server(sim_config.port);
    serverApp = server.Install(csmaNodes.Get(2));
    serverApp.Start(Seconds(1));
    serverApp.Stop(Seconds(sim_config.simTime + 1));

    MyUdpClientHelper client(csmaInterfaces.GetAddress(2), sim_config.port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(sim_config.packetInterval)));
    client.SetAttribute("IntervalJitter", StringValue("ns3::UniformRandomVariable[Min=-0.000025|Max=0.000075]"));
    client.SetAttribute("PacketSize", UintegerValue(sim_config.payloadSize));
    clientApp = client.Install(wifiStaNode);
    clientApp.Start(Seconds(1));
    clientApp.Stop(Seconds(sim_config.simTime + 1));

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

    // --- Mobility ---
    MobilityHelper mobility;

    // APs: fixed positions
    Ptr<ListPositionAllocator> apPositionAllocator = CreateObject<ListPositionAllocator>();
    for (const auto& apPos : sim_config.apPositions) {
        apPositionAllocator->Add(Vector(apPos.x, apPos.y, apPos.z));
    }
    mobility.SetPositionAllocator(apPositionAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    // STA: waypoint model (round trip: start → end → start per repetition)
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(wifiStaNode);
    Ptr<WaypointMobilityModel> staMobilityModel = DynamicCast<WaypointMobilityModel>(
        wifiStaNode.Get(0)->GetObject<MobilityModel>());
    staMobilityModel->AddWaypoint(Waypoint(Seconds(0),
        Vector(sim_config.staPosStart.x, sim_config.staPosStart.y, sim_config.staPosStart.z)));
    for (uint32_t i = 0; i < sim_config.repetitions; ++i) {
        // Forward leg: arrives at staPosEnd at t = tripTime*(2i+1)
        staMobilityModel->AddWaypoint(Waypoint(Seconds(sim_config.tripTime * (i*2 + 1)),
            Vector(sim_config.staPosEnd.x, sim_config.staPosEnd.y, sim_config.staPosEnd.z)));
        // Return leg: back at staPosStart at t = tripTime*(2i+2)
        staMobilityModel->AddWaypoint(Waypoint(Seconds(sim_config.tripTime * (i*2 + 2)),
            Vector(sim_config.staPosStart.x, sim_config.staPosStart.y, sim_config.staPosStart.z)));
    }

    // Interferers: fixed positions
    Ptr<ListPositionAllocator> intPositionAllocator = CreateObject<ListPositionAllocator>();
    for (const auto& inf : sim_config.interferers) {
        intPositionAllocator->Add(Vector(inf.position.x, inf.position.y, inf.position.z));
    }
    mobility.SetPositionAllocator(intPositionAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiInterfererNodes);

    // --- Loggers ---
    std::stringstream ss;
    ss << json(sim_config);
    STALogger sta_logger(out_config.STA_LOG_PATH, ss.str(),
        DynamicCast<WifiNetDevice>(staDevice.Get(0)), staMobilityModel);
    sta_logger.logHeader();

    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxPsduBegin";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&STALogger::sendingMpduCallback, &sta_logger));

    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/AckedMpdu";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&STALogger::ackedMpduCallback, &sta_logger));

    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/MpduResponseTimeout";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&STALogger::mpduTimeoutCallback, &sta_logger));

    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/DroppedMpdu";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&STALogger::droppedMpduCallback, &sta_logger));

    AssocLogger assoc_logger(out_config.ASSOC_LOG_PATH, "{\"header\": \"ADD PARAMETERS\"}", staMobilityModel);
    assoc_logger.logHeader();

    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&AssocLogger::assocCallback, &assoc_logger));

    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&AssocLogger::deAssocCallback, &assoc_logger));

    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/ReceivedBeaconInfo";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&AssocLogger::receivedBeaconInfoCallback, &assoc_logger));

    // Mobility polling
    Timer timer = Timer();
    timer.SetFunction(&timerCallback);
    timer.SetArguments(&timer, staMobilityModel);
    timer.Schedule(Seconds(1));

    // Channel-change roaming manager
    RoamingManager roaming_manager(DynamicCast<WifiNetDevice>(staDevice.Get(0)),
                                   staMobilityModel, sim_config.channels);
    if (sim_config.doubleChannel) {
        ss.str(std::string());
        ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc";
        Config::ConnectWithoutContext(ss.str(), MakeCallback(&RoamingManager::deAssocCallback, &roaming_manager));
    }

    // --- PCAP ---
    if (sim_config.enablePcap)
    {
        spectrumPhyHelper.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        spectrumPhyHelper.EnablePcap("handover-sta", staDevice);
        spectrumPhyHelper.EnablePcap("handover-ap0", apDevices.Get(0));
        spectrumPhyHelper.EnablePcap("handover-ap1", apDevices.Get(1));
        csma.EnablePcapAll("handover-csma", true);
    }

    // --- Run ---
    PopulateArpCache();
    Simulator::Stop(Seconds(sim_config.simTime));

    if (sim_config.enableAnimation) {
        AnimationInterface anim("handover_anim.xml");
    }

    auto start = std::chrono::high_resolution_clock::now();
    Simulator::Run();

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);
    sta_logger.logFooter(duration);
    assoc_logger.logFooter();

    Simulator::Destroy();
    return 0;
}
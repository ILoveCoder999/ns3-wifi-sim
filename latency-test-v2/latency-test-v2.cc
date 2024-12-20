#include "interferer-application-helper.h"
#include "packet-info.h"
#include "utils.h"
#include "sta-logger.h"
#include "sta-logger-power.h"

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
#include "ns3/wifi-mac.h"
#include "ns3/wifi-mpdu.h"
#include "ns3/wifi-net-device.h"
#include "my-udp-client-helper.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <unordered_map>

using json = nlohmann::json;

using namespace ns3;


const bool ENABLE_POWER_LOG = false;


int main(int argc, char** argv) {

    LogComponentEnable("StaLogger", LOG_LEVEL_DEBUG);

    constexpr uint32_t port = 9;
    std::string outFilePath = "db0.json";
    std::string jsonConfig = "conf.json";    
    bool inlineConfig = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("jsonConfig", "Json configuration", jsonConfig);
    cmd.AddValue("outFilePath", "Output file path", outFilePath);
    cmd.AddValue("inlineConfig", "Provide config inline", inlineConfig);
    cmd.Parse(argc, argv);

    std::cout << jsonConfig << " " << outFilePath << " " << inlineConfig << std::endl;

    // if (argc < 3)
    // {
    //     std::cerr << "Missing parameters" << std::endl;
    //     return 1;
    // }

    Arguments args;

    if(inlineConfig)
    {
        std::stringstream conf_stream(jsonConfig);
        conf_stream >> args;
    }
    else {
        std::ifstream arg_file;  
        arg_file.open(jsonConfig.c_str(), std::ios::in);
        if(!arg_file)
        {
            return 2;
        }
        arg_file >> args;
    }

    //args.staNode.remoteStationManager = "ns3::ParfWifiManager";

    // Set seed
    RngSeedManager::SetSeed(1);
    SeedManager::SetSeed(1);
    //Packet::EnablePrinting();

    // Create node containers for AP, STA and interferers
    NodeContainer wifiApNodes;
    wifiApNodes.Create(args.apNodes.size());
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiInterfererNodes;
    wifiInterfererNodes.Create(args.interfererNodes.size());

    // Create spectrum helpers for different physical configuration
    // (only one will be selected for the simulation)
    std::vector<SpectrumWifiPhyHelper> spectrumPhys;
    for (auto &phyConfig : args.phyConfigs)
    {
        ObjectFactory factory; // used to create objects from their name (useful for json configuration)
        SpectrumWifiPhyHelper spectrumPhyHelper;
        factory.SetTypeId(phyConfig.channel.propagationLossModel);
        Ptr<SpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
        Ptr<PropagationLossModel> propagationLossModel = DynamicCast<PropagationLossModel>(factory.Create());
        spectrumChannel->AddPropagationLossModel(propagationLossModel);
        factory.SetTypeId(phyConfig.channel.propagationDelayModel);
        Ptr<PropagationDelayModel> propagationDelayModel = DynamicCast<PropagationDelayModel>(factory.Create());
        spectrumChannel->SetPropagationDelayModel(propagationDelayModel);
        spectrumPhyHelper.SetChannel(spectrumChannel);
        spectrumPhyHelper.Set("ChannelSettings", StringValue(phyConfig.channelSettings));
        spectrumChannel->AssignStreams(100);

        spectrumPhys.push_back(spectrumPhyHelper);
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    WifiMacHelper wifiMac;

    NetDeviceContainer staDevice;
    NetDeviceContainer apDevices;
    NetDeviceContainer interfererDevices;

    // Set station manager (rate adaptation algorithms, e.g. Minstral)
    if (args.staNode.remoteStationManager == "ns3::ConstantRateWifiManager")
    {
        wifi.SetRemoteStationManager(args.staNode.remoteStationManager,
            "MaxSsrc", UintegerValue(21),
            "RtsCtsThreshold", UintegerValue(args.staNode.rtsCtsThreshold),
            "DataMode", StringValue(args.staNode.dataMode));
    }
    else
    {
        wifi.SetRemoteStationManager(args.staNode.remoteStationManager,
            "MaxSsrc", UintegerValue(21),
            "RtsCtsThreshold", UintegerValue(args.staNode.rtsCtsThreshold));
    }

    // Configure WiFi for STA (with beacons)
    // Install on STA node together with spectrum
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(args.staNode.ssid));
    staDevice = wifi.Install(spectrumPhys[args.staNode.phyId], wifiMac, wifiStaNode);

    // Configure and install STA Wifi on interferers
    for (long unsigned int i = 0;i < args.interfererNodes.size();i++)
    {
        const auto& interfererConfig = args.interfererNodes[i];
        const auto wifiInterferentNode = wifiInterfererNodes.Get(i);
        // Use STA WiFi config
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(interfererConfig.ssid));
        // Set rate adaptation algorithm
        if (interfererConfig.remoteStationManager == "ns3::ConstantRateWifiManager")
        {
            wifi.SetRemoteStationManager(interfererConfig.remoteStationManager,
                "MaxSsrc", UintegerValue(21),
                "RtsCtsThreshold", UintegerValue(interfererConfig.rtsCtsThreshold),
                "DataMode", StringValue(interfererConfig.dataMode));
        }
        else
        {
            wifi.SetRemoteStationManager(interfererConfig.remoteStationManager,
                "MaxSsrc", UintegerValue(21),
                "RtsCtsThreshold", UintegerValue(interfererConfig.rtsCtsThreshold));
        }
        interfererDevices.Add(wifi.Install(spectrumPhys[interfererConfig.phyId], wifiMac, wifiInterferentNode));
    }

    // Map SSID to node number for APs
    std::unordered_map<std::string, long unsigned int> apNodesLookup;

    // Configure WiFi for APs
    for (long unsigned int i = 0; i < args.apNodes.size(); i++)
    {
        const auto& apConfig = args.apNodes[i];
        const auto wifiApNode = wifiApNodes.Get(i);
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(apConfig.ssid));
        wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
        apDevices.Add(wifi.Install(spectrumPhys[apConfig.phyId], wifiMac, wifiApNode));
        apNodesLookup.insert({ apConfig.ssid, i });
    }

    // Enable pcap on all nodes (physical layer)
    if (args.enablePcap)
    {
        for (auto &spectrumPhy : spectrumPhys)
        {
            spectrumPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        }
        for (long unsigned int i = 0;i < args.apNodes.size();i++)
        {
            spectrumPhys[args.apNodes[i].phyId].EnablePcap(args.pcapPrefix + "latency-test-ap", apDevices.Get(i));
        }
        for (long unsigned int i = 0;i < args.interfererNodes.size();i++)
        {
            spectrumPhys[args.interfererNodes[i].phyId].EnablePcap(args.pcapPrefix + "latency-test-interferer", interfererDevices.Get(i));
        }
        spectrumPhys[args.staNode.phyId].EnablePcap(args.pcapPrefix + "latency-test-sta", staDevice);
    }

    // Create vector of position for APs, STA, and interferers
    Ptr<ListPositionAllocator> positionAllocator = CreateObject<ListPositionAllocator>();
    for (const auto &apConfig : args.apNodes)
    {
        positionAllocator->Add(Vector(apConfig.position.x, apConfig.position.y, apConfig.position.z));
    }

    positionAllocator->Add(Vector(args.staNode.position.x, args.staNode.position.y, args.staNode.position.z));

    for (const auto &interfererConf : args.interfererNodes)
    {
        positionAllocator->Add(Vector(interfererConf.position.x, interfererConf.position.y, interfererConf.position.z));
    }

    // Configure mobility model (with type and vector of positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Install mobility model on nodes (probably goes through the vector of positions)
    mobility.Install(wifiApNodes);
    mobility.Install(wifiStaNode);
    mobility.Install(wifiInterfererNodes);

    // Install internet stack
    InternetStackHelper internetStack;
    internetStack.Install(wifiApNodes);
    internetStack.Install(wifiStaNode);
    internetStack.Install(wifiInterfererNodes);

    // Configure IP addresses on same subnet
    Ipv4AddressHelper ipv4Address;
    ipv4Address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apNodesInterfaces;
    Ipv4InterfaceContainer staNodeInterface;
    Ipv4InterfaceContainer interfererNodesInterfaces;

    apNodesInterfaces = ipv4Address.Assign(apDevices);
    staNodeInterface = ipv4Address.Assign(staDevice);
    interfererNodesInterfaces = ipv4Address.Assign(interfererDevices);

    // Install applications on nodes
    ApplicationContainer clientApp;
    ApplicationContainer serverApp;
    ApplicationContainer interfererApps;

    // UDP server on APs (set start time)
    UdpServerHelper server(port);
    serverApp = server.Install(wifiApNodes);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(args.simulationTime + 1));

    // Custom UDP client on STA
    MyUdpClientHelper client(apNodesInterfaces.GetAddress(apNodesLookup[args.staNode.ssid]), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(args.staNode.packetInterval)));
    client.SetAttribute("IntervalJitter", StringValue("ns3::UniformRandomVariable[Min=-0.000025|Max=0.000075]"));
    client.SetAttribute("PacketSize", UintegerValue(args.staNode.payloadSize));
    clientApp = client.Install(wifiStaNode);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(args.simulationTime + 1));

    // Custom UDP client on interferers
    InterfererApplicationHelper interfererHelper;
    for (long unsigned int i = 0;i < args.interfererNodes.size();i++)
    {
        const auto &interfererConfig = args.interfererNodes[i];
        interfererHelper.SetAttribute("PeerAddress", Ipv4AddressValue(apNodesInterfaces.GetAddress(apNodesLookup[interfererConfig.ssid])));
        interfererHelper.SetAttribute("OffTime", StringValue(interfererConfig.offTime));
        interfererHelper.SetAttribute("BurstSize", StringValue(interfererConfig.burstSize));
        interfererHelper.SetAttribute("BurstPacketsInterval", TimeValue(MicroSeconds(interfererConfig.burstPacketsIntervalMicroSeconds)));
        interfererHelper.SetAttribute("BurstPacketsSize", UintegerValue(interfererConfig.burstPacketsSize));
        interfererApps.Add(interfererHelper.Install(wifiInterfererNodes.Get(i)));
    }
    interfererApps.Start(Seconds(1.0));
    interfererApps.Stop(Seconds(args.simulationTime + 1));


    // create STA logger
    STALogger sta_logger(outFilePath, args, DynamicCast<WifiNetDevice>(staDevice.Get(0)));

    // if (ENABLE_POWER_LOG)
    // {
    //     UintegerValue packet_size_obj;
    //     clientApp.Get(0)->GetAttribute("PacketSize", packet_size_obj);
    //     sta_logger = new STALoggerPower(outFilePath, args, DynamicCast<WifiNetDevice>(staDevice.Get(0)), packet_size_obj.Get());
    // }
    // else {
    //     sta_logger = new STALogger(outFilePath, args);
    // }
    sta_logger.logHeader();

    // Tracing for sent packets
    std::stringstream ss;
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

    // // Power tracing
    // if (ENABLE_POWER_LOG)
    // {
    //     std::string sta_manager = args.staNode.remoteStationManager;  
        
    //     if(sta_manager == "ns3::MinstrelHtWifiManager" || sta_manager == "ns3::MinstrelWifiManager")
    //     {
    //         //NS_FATAL_ERROR("STA power log not supported for selected WifiManager");
    //         ss.str(std::string());
    //         ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/$" + sta_manager + "/Rate";
    //         Config::Connect(ss.str(), MakeCallback(&STALoggerPower::rateChangeCallback, dynamic_cast<STALoggerPower*>(sta_logger)));

    //         ss.str(std::string());
    //         ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxBegin";
    //         Config::Connect(ss.str(), MakeCallback(&STALoggerPower::phyTxCallback, dynamic_cast<STALoggerPower*>(sta_logger)));
    //     }
    //     // Callbacks are different depending on WiFi Manager
    //     else {
    //         NS_FATAL_ERROR("STA power log not supported for selected WifiManager");

    //         ss.str(std::string());
    //         ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/$" + sta_manager + "/RateChange";
    //         Config::Connect(ss.str(), MakeCallback(&STALoggerPower::rateChangeCallbackDest, dynamic_cast<STALoggerPower*>(sta_logger)));

    //         ss.str(std::string());
    //         ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/$" + sta_manager + "/PowerChange";
    //         Config::Connect(ss.str(), MakeCallback(&STALoggerPower::powerChangeCallback, dynamic_cast<STALoggerPower*>(sta_logger)));

    //         ss.str(std::string());
    //         ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxBegin";
    //         Config::Connect(ss.str(), MakeCallback(&STALoggerPower::phyTxCallback, dynamic_cast<STALoggerPower*>(sta_logger)));
    //     }      
    // }

    // DOES NOT WORK IN DEBUG (ONLY IN RELEASE)
    PopulateArpCache();

    // Start simulation
    Simulator::Stop(Seconds(args.simulationTime + 1));
    auto start = std::chrono::high_resolution_clock::now();
    Simulator::Run();

    // Log duration of simulation
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

    sta_logger.logFooter(duration);
    
    // // delete sta logger
    // delete sta_logger;

    Simulator::Destroy();
    return 0;
}

/*NOTES

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

*/
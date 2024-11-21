#include "interference-application-helper.h"
#include "arguments.h"
#include "ns3/application-container.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/llc-snap-header.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/node-container.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/seq-ts-header.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-header.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-mpdu.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

using namespace ns3;

void
AckedMpduCallback(Ptr<const WifiMpdu> mpdu)
{
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;
    if (p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
    {
        std::cout << (Simulator::Now() - seqTsHeader.GetTs()).ToInteger(Time::Unit::NS) << std::endl;
    }
}

void
MpduTimeoutCallback(uint8_t reason,
                    Ptr<const WifiMpdu> mpdu,
                    const WifiTxVector& txVector)
{
    // Avviene nel caso di una ritrasmissione
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;
    if (p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
    {
        std::cout << "MPDU Timeout (" << seqTsHeader.GetSeq() << ") " << (Simulator::Now() - seqTsHeader.GetTs()).ToInteger(Time::Unit::NS) << std::endl;
    }
}

int
main(int argc, char** argv) {
    uint32_t interferenceNodes = 0;
    double apStaDistance = 20;
    double apInterferentDistanceX = 10;
    double apInterferentDistanceY = 10;
    uint32_t payloadSize = 50;
    uint16_t port = 9;
    double packetInterval = 0.5;
    double simulationTime = 10;
    std::string interferenceOffTime = "ns3::ExponentialRandomVariable[Mean=0.25|Bound=10]";
    std::string interferenceBurstSize = "ns3::ExponentialRandomVariable[Mean=100|Bound=500]";
    bool enablePcap = false;
    std::string pcapPrefix = "";
    uint32_t staRtsCtsThreshold = 4692480;
    uint32_t interferenceRtsCtsThreshold = 4692480;
    std::string staRemoteStationManager = "ns3::IdealWifiManager";
    std::string interferenceRemoteStationManager = "ns3::IdealWifiManager";
    std::string staDataMode = "OfdmRate6Mbps";
    std::string interferenceDataMode = "OfdmRate6Mbps";
    std::string jsonConfig = "";
    Arguments args;

    CommandLine cmd(__FILE__);
    cmd.AddValue("interferenceNodes", "Number of interferent stations.", interferenceNodes);
    cmd.AddValue("apStaDistance", "Distance in meters between the STA and the AP.", apStaDistance);
    cmd.AddValue("apInterferentDistanceX", "Distance in meters between the AP and the Interferent nodes on the X axis (main).", apInterferentDistanceX);
    cmd.AddValue("apInterferentDistanceY", "Distance in meters between the AP and the Interferent nodes on the Y axis.", apInterferentDistanceY);
    cmd.AddValue("payloadSize", "Size of the payload of the packets exchanged.", payloadSize);
    cmd.AddValue("packetInterval", "The interval between packets sent by the STA.", packetInterval);
    cmd.AddValue("simulationTime", "The duration of the simulation.", simulationTime);
    cmd.AddValue("interferenceOffTime", "The interval between burst sent by the interference nodes.", interferenceOffTime);
    cmd.AddValue("interferenceBurstSize", "The size of the bursts (number of packets) sent by interference nodes.", interferenceBurstSize);
    cmd.AddValue("staRtsCtsThreshold", "The RTS/CTS threshold in bytes for the STA node.", staRtsCtsThreshold);
    cmd.AddValue("interferenceRtsCtsThreshold", "The RTS/CTS threshold in bytes for the interferent nodes.", interferenceRtsCtsThreshold);
    cmd.AddValue("staRemoteStationManager", "The RemoteStationManager for the STA node.", staRemoteStationManager);
    cmd.AddValue("interferenceRemoteStationManager", "The RemoteStationManager for the interferent node.", interferenceRemoteStationManager);
    cmd.AddValue("staDataMode", "The constant data mode for a ns3::ConstantDataRateWifiManager of the STA node.", staDataMode);
    cmd.AddValue("interferenceDataMode", "The constant data mode for a ns3::ConstantDataRateWifiManager of the interferent nodes.", interferenceDataMode);
    cmd.AddValue("enablePcap", "Enables the collection of pcap files.", enablePcap);
    cmd.AddValue("pcapPrefix", "Prefix for pcap files.", pcapPrefix);
    cmd.AddValue("jsonConfig", "Json advanced configuration.", jsonConfig);
    cmd.Parse(argc, argv);

    if (!jsonConfig.empty())
    {
        json j = json::parse(jsonConfig);
        args = j.get<Arguments>();
    }
    else
    {
        args.sta.position.x = apStaDistance;
        args.sta.payloadSize = payloadSize;
        args.sta.packetInterval = packetInterval;
        args.sta.rtsCtsThreshold = staRtsCtsThreshold;
        args.sta.remoteStationManager = staRemoteStationManager;
        args.sta.dataMode = staDataMode;

        args.interferent.clear();
        for (uint32_t i = 0;i < interferenceNodes;i++)
        {
            InterferentConfig conf;
            conf.position.x = apInterferentDistanceX;
            conf.position.y = (i % 2 ? 1 : -1) * apInterferentDistanceY;
            conf.burstSize = interferenceBurstSize;
            conf.offTime = interferenceOffTime;
            conf.remoteStationManager = interferenceRemoteStationManager;
            conf.dataMode = interferenceDataMode;
            args.interferent.push_back(conf);
        }

        args.simulationTime = simulationTime;
        args.enablePcap = enablePcap;
        args.pcapPrefix = pcapPrefix;
    }

    RngSeedManager::SetSeed(1);
    SeedManager::SetSeed(1);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiInterferenceNodes;
    wifiInterferenceNodes.Create(args.interferent.size());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns380211a");

    NetDeviceContainer staDevice;
    NetDeviceContainer apDevice;
    NetDeviceContainer interferenceDevices;

    SpectrumWifiPhyHelper spectrumPhy;

    Ptr<SpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<PropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel>();
    Ptr<PropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    spectrumChannel->AddPropagationLossModel(lossModel);
    spectrumChannel->SetPropagationDelayModel(delayModel);
    spectrumPhy.SetChannel(spectrumChannel);
    //spectrumPhy.SetErrorRateModel("ns3::NistErrorRateModel");
    spectrumChannel->AssignStreams(100);

    if (args.sta.remoteStationManager == "ns3::ConstantRateWifiManager")
    {
        wifi.SetRemoteStationManager(args.sta.remoteStationManager,
            "MaxSsrc", UintegerValue(21),
            "RtsCtsThreshold", UintegerValue(args.sta.rtsCtsThreshold),
            "DataMode", StringValue(args.sta.dataMode));
    }
    else
    {
        wifi.SetRemoteStationManager(args.sta.remoteStationManager,
            "MaxSsrc", UintegerValue(21),
            "RtsCtsThreshold", UintegerValue(args.sta.rtsCtsThreshold));
    }

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    spectrumPhy.Set("ChannelSettings", StringValue("{44, 20, BAND_5GHZ, 0}"));
    staDevice = wifi.Install(spectrumPhy, wifiMac, wifiStaNode);

    for (long unsigned int i = 0;i < args.interferent.size();i++)
    {
        const auto& interferentConfig = args.interferent[i];
        const auto wifiInterferenceNode = wifiInterferenceNodes.Get(i);
        if (interferentConfig.remoteStationManager == "ns3::ConstantRateWifiManager")
        {
            wifi.SetRemoteStationManager(staRemoteStationManager,
                "MaxSsrc", UintegerValue(21),
                "RtsCtsThreshold", UintegerValue(interferentConfig.rtsCtsThreshold),
                "DataMode", StringValue(interferentConfig.dataMode));
        }
        else
        {
            wifi.SetRemoteStationManager(interferenceRemoteStationManager,
                "MaxSsrc", UintegerValue(21),
                "RtsCtsThreshold", UintegerValue(interferentConfig.rtsCtsThreshold));
        }
        interferenceDevices.Add(wifi.Install(spectrumPhy, wifiMac, wifiInterferenceNode));
    }

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
    apDevice = wifi.Install(spectrumPhy, wifiMac, wifiApNode);

    if (args.enablePcap)
    {
        spectrumPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        spectrumPhy.EnablePcap(args.pcapPrefix + "latency-test-ap", apDevice);
        spectrumPhy.EnablePcap(args.pcapPrefix + "latency-test-sta", staDevice);
        spectrumPhy.EnablePcap(args.pcapPrefix + "latency-test-interference", interferenceDevices);
    }

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> apStaPositionAllocator = CreateObject<ListPositionAllocator>();
    apStaPositionAllocator->Add(Vector(0.0, 0.0, 0.0));
    apStaPositionAllocator->Add(Vector(args.sta.position.x, args.sta.position.y, args.sta.position.z));

    Ptr<ListPositionAllocator> interferencePositionAllocator = CreateObject<ListPositionAllocator>();
    for (const auto &interferentConf : args.interferent)
    {
        interferencePositionAllocator->Add(Vector(interferentConf.position.x, interferentConf.position.y, interferentConf.position.y));
    }

    mobility.SetPositionAllocator(apStaPositionAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);

    mobility.SetPositionAllocator(interferencePositionAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiInterferenceNodes);

    InternetStackHelper internetStack;
    internetStack.Install(wifiApNode);
    internetStack.Install(wifiStaNode);
    internetStack.Install(wifiInterferenceNodes);

    Ipv4AddressHelper ipv4Address;
    ipv4Address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apNodeInterface;
    Ipv4InterfaceContainer staNodeInterface;
    Ipv4InterfaceContainer interferenceNodesInterfaces;

    apNodeInterface = ipv4Address.Assign(apDevice);
    staNodeInterface = ipv4Address.Assign(staDevice);
    interferenceNodesInterfaces = ipv4Address.Assign(interferenceDevices);

    ApplicationContainer clientApp;
    ApplicationContainer serverApp;
    ApplicationContainer interferenceApps;

    UdpServerHelper server(port);
    serverApp = server.Install(wifiApNode);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(args.simulationTime + 1));

    UdpClientHelper client(apNodeInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(args.sta.packetInterval)));
    client.SetAttribute("PacketSize", UintegerValue(args.sta.payloadSize));
    clientApp = client.Install(wifiStaNode);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(args.simulationTime + 1));

    InterferenceApplicationHelper interferenceHelper;
    interferenceHelper.SetAttribute("PeerAddress", Ipv4AddressValue(apNodeInterface.GetAddress(0)));
    interferenceApps = interferenceHelper.Install(wifiInterferenceNodes);
    interferenceApps.Start(Seconds(1.0));
    interferenceApps.Stop(Seconds(args.simulationTime + 1));

    std::stringstream ss;
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/AckedMpdu";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&AckedMpduCallback));


    std::stringstream ss2;
    ss2 << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/MpduResponseTimeout";
    Config::ConnectWithoutContext(ss2.str(), MakeCallback(&MpduTimeoutCallback));

    Simulator::Stop(Seconds(args.simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
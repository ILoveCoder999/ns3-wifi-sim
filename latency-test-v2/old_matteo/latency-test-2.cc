#include "arguments.h"
#include "interferer-application-helper.h"
#include "packet-info.h"
#include "utils.h"

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
#include "my-udp-client-helper.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <unordered_map>

using json = nlohmann::json;

using namespace ns3;

std::unordered_map<uint32_t, PacketInfo> packets;

void
AckedMpduCallback(Ptr<OutputStreamWrapper> out,
                  Ptr<const WifiMpdu> mpdu)
{
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;
    if (p->GetSize() > 0 && p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
    {
        PacketInfo info {seqTsHeader.GetSeq()};
        const auto it = packets.find(seqTsHeader.GetSeq());
        if (it != packets.end())
        {
            info = it->second;
        }
        info.acked = true;
        info.latency = Simulator::Now() - seqTsHeader.GetTs();
        *out->GetStream() << ", " << std::endl << json(info);
        packets.erase(it, packets.end());
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
    if (p->GetSize() > 0 && p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
    {
        PacketInfo info {seqTsHeader.GetSeq()};
        auto it = packets.find(seqTsHeader.GetSeq());
        if (it != packets.end())
        {
            info = it->second;
        }
        info.retransmissions.push_back(RetransmissionInfo {
            txVector.GetMode().GetDataRate(txVector),
            Simulator::Now() - seqTsHeader.GetTs()
        });
        packets.insert_or_assign(seqTsHeader.GetSeq(), info);
    }
}

void
DroppedMpduCallback(Ptr<OutputStreamWrapper> out,
                    WifiMacDropReason reason,
                    Ptr<const WifiMpdu> mpdu)
{
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;
    if (p->GetSize() > 0 && p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
    {
        PacketInfo info {seqTsHeader.GetSeq()};
        const auto it = packets.find(seqTsHeader.GetSeq());
        if (it != packets.end())
        {
            info = it->second;
        }
        info.acked = false;
        info.latency = Simulator::Now() - seqTsHeader.GetTs();
        *out->GetStream() << ", " << std::endl << json(info);
        packets.erase(it, packets.end());
    }
}

int
main(int argc, char** argv) {
    constexpr uint32_t port = 9;
    std::string outFilePath = "db0.json";
    Arguments args;

    if (argc < 3)
    {
        std::cerr << "Missing parameters" << std::endl;
        return 1;
    }

    CommandLine cmd(__FILE__);
    cmd.AddNonOption("jsonConfig", "Json configuration", args);
    cmd.AddNonOption("outFilePath", "Output file path", outFilePath);
    cmd.Parse(argc, argv);

    AsciiTraceHelper asciiHelper;
    Ptr<OutputStreamWrapper> outputFile = asciiHelper.CreateFileStream(outFilePath, std::ios::out | std::ios::trunc);
    RngSeedManager::SetSeed(1);
    SeedManager::SetSeed(1);
    //Packet::EnablePrinting();

    *outputFile->GetStream() << "[" << std::endl << json(args);

    NodeContainer wifiApNodes;
    wifiApNodes.Create(args.apNodes.size());
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiInterfererNodes;
    wifiInterfererNodes.Create(args.interfererNodes.size());

    std::unordered_map<std::string, long unsigned int> apNodesLookup;
    std::vector<SpectrumWifiPhyHelper> spectrumPhys;
    for (auto &phyConfig : args.phyConfigs)
    {
        ObjectFactory factory;
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

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(args.staNode.ssid));
    staDevice = wifi.Install(spectrumPhys[args.staNode.phyId], wifiMac, wifiStaNode);

    for (long unsigned int i = 0;i < args.interfererNodes.size();i++)
    {
        const auto& interfererConfig = args.interfererNodes[i];
        const auto wifiInterferentNode = wifiInterfererNodes.Get(i);
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(interfererConfig.ssid));
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

    for (long unsigned int i = 0;i < args.apNodes.size();i++)
    {
        const auto& apConfig = args.apNodes[i];
        const auto wifiApNode = wifiApNodes.Get(i);
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(apConfig.ssid));
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue("OfdmRate24Mbps"));
        apDevices.Add(wifi.Install(spectrumPhys[apConfig.phyId], wifiMac, wifiApNode));
        apNodesLookup.insert({ apConfig.ssid, i });
    }

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

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAllocator = CreateObject<ListPositionAllocator>();
    for (const auto &apConfig : args.apNodes)
    {
        positionAllocator->Add(Vector(apConfig.position.x, apConfig.position.y, apConfig.position.z));
    }

    positionAllocator->Add(Vector(args.staNode.position.x, args.staNode.position.y, args.staNode.position.z));

    for (const auto &interfererConf : args.interfererNodes)
    {
        positionAllocator->Add(Vector(interfererConf.position.x, interfererConf.position.y, interfererConf.position.y));
    }

    mobility.SetPositionAllocator(positionAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiApNodes);
    mobility.Install(wifiStaNode);
    mobility.Install(wifiInterfererNodes);

    InternetStackHelper internetStack;
    internetStack.Install(wifiApNodes);
    internetStack.Install(wifiStaNode);
    internetStack.Install(wifiInterfererNodes);

    Ipv4AddressHelper ipv4Address;
    ipv4Address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apNodesInterfaces;
    Ipv4InterfaceContainer staNodeInterface;
    Ipv4InterfaceContainer interfererNodesInterfaces;

    apNodesInterfaces = ipv4Address.Assign(apDevices);
    staNodeInterface = ipv4Address.Assign(staDevice);
    interfererNodesInterfaces = ipv4Address.Assign(interfererDevices);

    ApplicationContainer clientApp;
    ApplicationContainer serverApp;
    ApplicationContainer interfererApps;

    UdpServerHelper server(port);
    serverApp = server.Install(wifiApNodes);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(args.simulationTime + 1));

    MyUdpClientHelper client(apNodesInterfaces.GetAddress(apNodesLookup[args.staNode.ssid]), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(args.staNode.packetInterval)));
    client.SetAttribute("IntervalJitter", StringValue("ns3::UniformRandomVariable[Min=-0.000025|Max=0.000075]"));
    client.SetAttribute("PacketSize", UintegerValue(args.staNode.payloadSize));
    clientApp = client.Install(wifiStaNode);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(args.simulationTime + 1));

    // Added interferences
    ApplicationContainer staServer = server.Install(wifiStaNode);
    staServer.Start(Seconds(1.0));
    staServer.Stop(Seconds(args.simulationTime + 1));

    MyUdpClientHelper httpInterferer(staNodeInterface.GetAddress(0), port);
    httpInterferer.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    httpInterferer.SetAttribute("Interval", TimeValue(Seconds(60)));
    httpInterferer.SetAttribute("IntervalJitter", StringValue("ns3::UniformRandomVariable[Min=-0.0001|Max=0.0001]"));
    httpInterferer.SetAttribute("PacketSize", UintegerValue(3042));
    ApplicationContainer httpInt = httpInterferer.Install(wifiApNodes);
    httpInt.Start(Seconds(1.0));
    httpInt.Stop(Seconds(args.simulationTime + 1));

    MyUdpClientHelper stpInterferer(staNodeInterface.GetAddress(0), port);
    stpInterferer.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    stpInterferer.SetAttribute("Interval", TimeValue(Seconds(2)));
    stpInterferer.SetAttribute("IntervalJitter", StringValue("ns3::UniformRandomVariable[Min=-0.0001|Max=0.0001]"));
    stpInterferer.SetAttribute("PacketSize", UintegerValue(12));
    ApplicationContainer stpInt = stpInterferer.Install(wifiApNodes);
    stpInt.Start(Seconds(1.0));
    stpInt.Stop(Seconds(args.simulationTime + 1));

    InterfererApplicationHelper probInterferer;
    probInterferer.SetAttribute("PeerAddress", Ipv4AddressValue(apNodesInterfaces.GetAddress(apNodesLookup[args.apNodes[0].ssid])));
    probInterferer.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=3.333|Bound=20]"));
    probInterferer.SetAttribute("BurstSize", StringValue("ns3::ConstantRandomVariable"));
    probInterferer.SetAttribute("BurstPacketsInterval", TimeValue(Seconds(0)));
    probInterferer.SetAttribute("BurstPacketsSize", UintegerValue(18));
    NodeContainer probInterfererNode(1);
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(args.apNodes[0].ssid));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
        "DataMode", StringValue("OfdmRate6Mbps"));
    NetDeviceContainer probInterfererDevice = wifi.Install(spectrumPhys[0], wifiMac, probInterfererNode);
    internetStack.Install(probInterfererNode);
    ipv4Address.Assign(probInterfererDevice);
    ApplicationContainer probInt = probInterferer.Install(probInterfererNode);
    probInt.Start(Seconds(1.0));
    probInt.Stop(Seconds(args.simulationTime + 1));

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

    std::stringstream ss;
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/AckedMpdu";
    Config::ConnectWithoutContext(ss.str(), MakeBoundCallback(&AckedMpduCallback, outputFile));

    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/MpduResponseTimeout";
    Config::ConnectWithoutContext(ss.str(), MakeCallback(&MpduTimeoutCallback));

    ss.str(std::string());
    ss << "/NodeList/" << wifiStaNode.Get(0)->GetId() << "/DeviceList/0/Mac/DroppedMpdu";
    Config::ConnectWithoutContext(ss.str(), MakeBoundCallback(&DroppedMpduCallback, outputFile));

    PopulateArpCache();

    Simulator::Stop(Seconds(args.simulationTime + 1));
    auto start = std::chrono::high_resolution_clock::now();
    Simulator::Run();
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

    *outputFile->GetStream() << "," << std::endl << json({
        {"elapsed_seconds",  duration.count()}
    }) << std::endl << "]";

    Simulator::Destroy();
    return 0;
}
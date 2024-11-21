/*
 * Copyright (c) 2022
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Sebastien Deronne <sebastien.deronne@gmail.com>
 */

#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/eht-phy.h"
#include "ns3/enum.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-acknowledgment.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mpdu.h"
#include "my-test-application.h"

#include <array>
#include <functional>
#include <numeric>

// This is a simple example in order to show how to configure an IEEE 802.11be Wi-Fi network.
//
// It outputs the UDP or TCP goodput for every EHT MCS value, which depends on the MCS value (0 to
// 13), the channel width (20, 40, 80 or 160 MHz) and the guard interval (800ns, 1600ns or 3200ns).
// The PHY bitrate is constant over all the simulation run. The user can also specify the distance
// between the access point and the station: the larger the distance the smaller the goodput.
//
// The simulation assumes a configurable number of stations in an infrastructure network:
//
//  STA     AP
//    *     *
//    |     |
//   n1     n2
//
// Packets in this simulation belong to BestEffort Access Class (AC_BE).
// By selecting an acknowledgment sequence for DL MU PPDUs, it is possible to aggregate a
// Round Robin scheduler to the AP, so that DL MU PPDUs are sent by the AP via DL OFDMA.

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("my-test-simulation");

void
traceAckedMpdu(Ptr<const WifiMpdu> p)
{
    std::cout << "Latency(" << p->GetPacket()->GetUid() << "): " << (Simulator::Now() - p->GetTimestamp()) << std::endl;
}

void
RxLatency(uint32_t packetId, Time latency)
{
    std::cout << "Latency(" << packetId << "): " << latency << std::endl;
}

int
main(int argc, char* argv[])
{
    bool downlink{true};
    bool useRts{false};
    uint16_t mpduBufferSize{512};
    std::string emlsrLinks;
    uint16_t paddingDelayUsec{32};
    uint16_t transitionDelayUsec{128};
    uint16_t channelSwitchDelayUsec{100};
    bool switchAuxPhy{true};
    double simulationTime{10}; // seconds
    double frequency{5};       // whether the first link operates in the 2.4, 5 or 6 GHz
    double frequency2{0}; // whether the second link operates in the 2.4, 5 or 6 GHz (0 means no second link exists)
    double frequency3{0}; // whether the third link operates in the 2.4, 5 or 6 GHz (0 means no third link exists)
    std::size_t nStations{1};
    std::string dlAckSeqType{"NO-OFDMA"};
    bool enableUlOfdma{false};
    bool enableBsrp{false};
    uint32_t payloadSize = 50; // must fit in the max TX duration when transmitting at MCS 0 over an RU of 26 tones
    Time accessReqInterval{0};
    double roomWidth{10.0}; // width of the room
    double roomHeight{10.0}; // height of the room
    bool verbose{false};

    CommandLine cmd(__FILE__);
    cmd.AddValue(
        "frequency",
        "Whether the first link operates in the 2.4, 5 or 6 GHz band (other values gets rejected)",
        frequency);
    cmd.AddValue(
        "frequency2",
        "Whether the second link operates in the 2.4, 5 or 6 GHz band (0 means the device has one "
        "link, otherwise the band must be different than first link and third link)",
        frequency2);
    cmd.AddValue(
        "frequency3",
        "Whether the third link operates in the 2.4, 5 or 6 GHz band (0 means the device has up to "
        "two links, otherwise the band must be different than first link and second link)",
        frequency3);
    cmd.AddValue("emlsrLinks",
                 "The comma separated list of IDs of EMLSR links (for MLDs only)",
                 emlsrLinks);
    cmd.AddValue("emlsrPaddingDelay",
                 "The EMLSR padding delay in microseconds (0, 32, 64, 128 or 256)",
                 paddingDelayUsec);
    cmd.AddValue("emlsrTransitionDelay",
                 "The EMLSR transition delay in microseconds (0, 16, 32, 64, 128 or 256)",
                 transitionDelayUsec);
    cmd.AddValue("emlsrAuxSwitch",
                 "Whether Aux PHY should switch channel to operate on the link on which "
                 "the Main PHY was operating before moving to the link of the Aux PHY. ",
                 switchAuxPhy);
    cmd.AddValue("channelSwitchDelay",
                 "The PHY channel switch delay in microseconds",
                 channelSwitchDelayUsec);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("downlink",
                 "Generate downlink flows if set to 1, uplink flows otherwise",
                 downlink);
    cmd.AddValue("useRts", "Enable/disable RTS/CTS", useRts);
    cmd.AddValue("mpduBufferSize",
                 "Size (in number of MPDUs) of the BlockAck buffer",
                 mpduBufferSize);
    cmd.AddValue("nStations", "Number of non-AP HE stations", nStations);
    cmd.AddValue("dlAckType",
                 "Ack sequence type for DL OFDMA (NO-OFDMA, ACK-SU-FORMAT, MU-BAR, AGGR-MU-BAR)",
                 dlAckSeqType);
    cmd.AddValue("enableUlOfdma",
                 "Enable UL OFDMA (useful if DL OFDMA is enabled and TCP is used)",
                 enableUlOfdma);
    cmd.AddValue("enableBsrp",
                 "Enable BSRP (useful if DL and UL OFDMA are enabled and TCP is used)",
                 enableBsrp);
    cmd.AddValue(
        "muSchedAccessReqInterval",
        "Duration of the interval between two requests for channel access made by the MU scheduler",
        accessReqInterval);
    cmd.AddValue("payloadSize", "The application payload size in bytes", payloadSize);
    cmd.AddValue("roomWidth", "The width of the room (2D)", roomWidth);
    cmd.AddValue("roomHeight", "The height of the room (2D)", roomHeight);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.Parse(argc, argv);

    if (verbose) 
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    if (useRts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
        Config::SetDefault("ns3::WifiDefaultProtectionManager::EnableMuRts", BooleanValue(true));
    }

    if (dlAckSeqType == "ACK-SU-FORMAT")
    {
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType",
                           EnumValue(WifiAcknowledgment::DL_MU_BAR_BA_SEQUENCE));
    }
    else if (dlAckSeqType == "MU-BAR")
    {
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType",
                           EnumValue(WifiAcknowledgment::DL_MU_TF_MU_BAR));
    }
    else if (dlAckSeqType == "AGGR-MU-BAR")
    {
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType",
                           EnumValue(WifiAcknowledgment::DL_MU_AGGREGATE_TF));
    }
    else if (dlAckSeqType != "NO-OFDMA")
    {
        NS_ABORT_MSG("Invalid DL ack sequence type (must be NO-OFDMA, ACK-SU-FORMAT, MU-BAR or "
                     "AGGR-MU-BAR)");
    }

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NetDeviceContainer apDevice;
    NetDeviceContainer staDevices;
    WifiMacHelper mac;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211be);
    std::array<std::string, 3> channelStr;
    std::array<FrequencyRange, 3> freqRanges;
    uint8_t nLinks = 0;

    if (frequency2 == frequency || frequency3 == frequency ||
        (frequency3 != 0 && frequency3 == frequency2))
    {
        std::cout << "Frequency values must be unique!" << std::endl;
        return 0;
    }

    for (auto freq : {frequency, frequency2, frequency3})
    {
        if (nLinks > 0 && freq == 0)
        {
            break;
        }
        channelStr[nLinks] = "{0, 0, ";
        if (freq == 6)
        {
            channelStr[nLinks] += "BAND_6GHZ, 0}";
            freqRanges[nLinks] = WIFI_SPECTRUM_6_GHZ;
            Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss",
                                DoubleValue(48));
        }
        else if (freq == 5)
        {
            channelStr[nLinks] += "BAND_5GHZ, 0}";
            freqRanges[nLinks] = WIFI_SPECTRUM_5_GHZ;
        }
        else if (freq == 2.4)
        {
            channelStr[nLinks] += "BAND_2_4GHZ, 0}";
            freqRanges[nLinks] = WIFI_SPECTRUM_2_4_GHZ;
            Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss",
                                DoubleValue(40));
        }
        else
        {
            std::cout << "Wrong frequency value!" << std::endl;
            return 0;
        }
        nLinks++;
    }

    if (nLinks > 1 && !emlsrLinks.empty())
    {
        wifi.ConfigEhtOptions("EmlsrActivated", BooleanValue(true));
    }

    Ssid ssid = Ssid("ns3-80211be");

    /*
        * SingleModelSpectrumChannel cannot be used with 802.11be because two
        * spectrum models are required: one with 78.125 kHz bands for HE PPDUs
        * and one with 312.5 kHz bands for, e.g., non-HT PPDUs (for more details,
        * see issue #408 (CLOSED))
        */
    SpectrumWifiPhyHelper phy(nLinks);
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.Set("ChannelSwitchDelay", TimeValue(MicroSeconds(channelSwitchDelayUsec)));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    mac.SetEmlsrManager("ns3::DefaultEmlsrManager",
                        "EmlsrLinkSet",
                        StringValue(emlsrLinks),
                        "EmlsrPaddingDelay",
                        TimeValue(MicroSeconds(paddingDelayUsec)),
                        "EmlsrTransitionDelay",
                        TimeValue(MicroSeconds(transitionDelayUsec)),
                        "SwitchAuxPhy",
                        BooleanValue(switchAuxPhy));
    for (uint8_t linkId = 0; linkId < nLinks; linkId++)
    {
        phy.Set(linkId, "ChannelSettings", StringValue(channelStr[linkId]));

        auto spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
        auto lossModel = CreateObject<LogDistancePropagationLossModel>();
        spectrumChannel->AddPropagationLossModel(lossModel);
        phy.AddChannel(spectrumChannel, freqRanges[linkId]);
    }
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    if (dlAckSeqType != "NO-OFDMA")
    {
        mac.SetMultiUserScheduler("ns3::RrMultiUserScheduler",
                                    "EnableUlOfdma",
                                    BooleanValue(enableUlOfdma),
                                    "EnableBsrp",
                                    BooleanValue(enableBsrp),
                                    "AccessReqInterval",
                                    TimeValue(accessReqInterval));
    }
    mac.SetType("ns3::ApWifiMac",
                "EnableBeaconJitter",
                BooleanValue(false),
                "Ssid",
                SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(1);
    int64_t streamNumber = 100;
    streamNumber += wifi.AssignStreams(apDevice, streamNumber);
    streamNumber += wifi.AssignStreams(staDevices, streamNumber);

    // mobility.
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator", 
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(roomWidth) + "]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(roomHeight) + "]"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    /* Internet stack*/
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterfaces;
    Ipv4InterfaceContainer apNodeInterface;

    staNodeInterfaces = address.Assign(staDevices);
    apNodeInterface = address.Assign(apDevice);

    /* Setting applications */
    //ApplicationContainer serverApp;
    //auto serverNodes = downlink ? std::ref(wifiStaNodes) : std::ref(wifiApNode);
    Ipv4InterfaceContainer serverInterfaces;
    NodeContainer clientNodes;
    NodeContainer serverNodes;
    for (std::size_t i = 0; i < nStations; i++)
    {
        if (downlink)
        {
            serverInterfaces.Add(staNodeInterfaces.Get(i));
            clientNodes.Add(wifiApNode.Get(0));
            serverNodes.Add(wifiStaNodes.Get(i));
        }
        else
        {
            serverInterfaces.Add(apNodeInterface.Get(0));
            clientNodes.Add(wifiStaNodes.Get(i));
            serverNodes.Add(wifiApNode.Get(0));
        }
    }

    // UDP flow
    uint16_t port = 8080;

    for (std::size_t i = 0; i < nStations; i++)
    {
        Ptr<TestApplication> client = CreateObject<TestApplication>();
        client->SetAttribute("PeerAddress", AddressValue(serverInterfaces.GetAddress(i)));
        client->SetAttribute("PeerPort", UintegerValue(port));
        ApplicationContainer clientApp = (Ptr<Application>)client;
        clientNodes.Get(i)->AddApplication(client);
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime + 1));
        client->TraceConnectWithoutContext("RxLatency", MakeCallback(&RxLatency));

        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(serverInterfaces.GetAddress(i), port));
        ApplicationContainer serverApp = sinkHelper.Install(serverNodes.Get(i));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime + 1));
    }

    phy.EnablePcap("my-test", downlink ? staDevices.Get(0) : apDevice.Get(0), true);

    Packet::EnablePrinting();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}

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
#include "my-test-application-helper.h"

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
RxLatency(Ptr<OutputStreamWrapper> osw, TestApplication::LatencyCallbackArgs args)
{
    args.Print(*osw->GetStream());
    //*osw->GetStream() << args.packet_id << " " << args.channel_number << " " << args.rtt.ToInteger(Time::Unit::NS) << " " << args.queue_n_packets << " " << args.signalNoise.signal << " " << args.signalNoise.noise << std::endl;
}

int
main(int argc, char* argv[])
{
    double simulationTime{10}; // seconds
    std::size_t nStations{4};
    uint32_t payloadSize = 50; // must fit in the max TX duration when transmitting at MCS 0 over an RU of 26 tones
    uint32_t burstSize = 1;
    double roomWidth{10.0}; // width of the room
    double roomHeight{10.0}; // height of the room
    bool verbose{false};

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("nStations", "Number of non-AP HE stations", nStations);
    cmd.AddValue("payloadSize", "The application payload size in bytes", payloadSize);
    cmd.AddValue("burstSize", "The burst size", burstSize);
    cmd.AddValue("roomWidth", "The width of the room (2D)", roomWidth);
    cmd.AddValue("roomHeight", "The height of the room (2D)", roomHeight);
    cmd.AddValue("verbose", "verbose output", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("my-test-simulation", LOG_LEVEL_DEBUG);
        LogComponentEnable("TestApplication", LOG_LEVEL_DEBUG);
    }

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(1);

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;
    WifiMacHelper mac;
    WifiHelper wifi;

    Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue(40));
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(900));
    // //Otherwise packet loss will occurr
    Config::SetDefault("ns3::ArpCache::PendingQueueSize", UintegerValue(burstSize));

    wifi.SetStandard(WIFI_STANDARD_80211n);
    SpectrumWifiPhyHelper phySta(1);
    phySta.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ap_ch1")));

    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel>();
    spectrumChannel->AddPropagationLossModel(lossModel);
    phySta.Set(0, "ChannelSettings", StringValue("{36, 20, BAND_5GHZ, 0}"));
    phySta.AddChannel(spectrumChannel, WIFI_SPECTRUM_5_GHZ);
    // phySta.Set(0, "ChannelSettings", StringValue("{1, 20, BAND_2_4GHZ, 0}"));
    // phySta.AddChannel(spectrumChannel, WIFI_SPECTRUM_2_4_GHZ);
    staDevices = wifi.Install(phySta, mac, wifiStaNodes);

    SpectrumWifiPhyHelper phyAp(1);
    phyAp.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phyAp.Set(0, "ChannelSettings", StringValue("{36, 20, BAND_5GHZ, 0}"));
    phyAp.AddChannel(spectrumChannel, WIFI_SPECTRUM_5_GHZ);
    // phyAp.Set(0, "ChannelSettings", StringValue("{1, 20, BAND_2_4GHZ, 0}"));
    // phyAp.AddChannel(spectrumChannel, WIFI_SPECTRUM_2_4_GHZ);

    mac.SetType("ns3::ApWifiMac",
                "EnableBeaconJitter",
                BooleanValue(false),
                "Ssid",
                SsidValue(Ssid("ap_ch1")));
    apDevices = wifi.Install(phyAp, mac, wifiApNodes);
    
    phyAp.EnablePcap("test-ap", wifiApNodes);
    phySta.EnablePcap("test-sta", wifiStaNodes);

    int64_t streamNumber = 100;
    streamNumber += wifi.AssignStreams(apDevices, streamNumber);
    streamNumber += wifi.AssignStreams(staDevices, streamNumber);

    // mobility.
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator", 
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(roomWidth) + "]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(roomHeight) + "]"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    
    mobility.Install(wifiApNodes);
    mobility.Install(wifiStaNodes);

    /* Internet stack*/
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes);
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeInterfaces = address.Assign(apDevices);


    /* Setting applications */
    // UDP flow
    uint16_t port = 9;
    TestApplicationHelper appHelper(apNodeInterfaces.GetAddress(0), port);
    appHelper.SetAttribute("BurstSize", UintegerValue(burstSize));
    appHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApplications = appHelper.Install(wifiStaNodes);

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(apNodeInterfaces.GetAddress(0), port));
    ApplicationContainer serverApplications = sinkHelper.Install(wifiApNodes);

    clientApplications.Start(Seconds(1.0));
    clientApplications.Stop(Seconds(simulationTime + 1));
    serverApplications.Start(Seconds(0.0));
    serverApplications.Stop(Seconds(simulationTime + 1));

    std::vector<std::ofstream> ofiles;
    for (std::size_t i = 0;i < nStations;i++)
    {
        AsciiTraceHelper asciiTraceHelper;
        Ptr<OutputStreamWrapper> osw = asciiTraceHelper.CreateFileStream("db" + std::to_string(i) + ".dat");
        *osw->GetStream() << "node_id channel_number channel_width latency_ns packets_in_queue signal_dbm noise_dbm" << std::endl;
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/ApplicationList/*/$TestApplication/RxLatency", MakeBoundCallback(&RxLatency, osw));
    }

    

    Packet::EnablePrinting();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}

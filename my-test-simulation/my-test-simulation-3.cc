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

const std::array<Ssid, 4> ssids = {
 Ssid("ap_ch1_1"),
 Ssid("ap_ch1_2"),
 Ssid("ap_ch1_3"),
 Ssid("ap_ch1_4")
};

void
traceAckedMpdu(Ptr<const WifiMpdu> p)
{
    std::cout << "Latency(" << p->GetPacket()->GetUid() << "): " << (Simulator::Now() - p->GetTimestamp()) << std::endl;
}

void
RxLatency(int id, uint32_t packetId, Time latency)
{
    std::cout << "[" << id << "]Latency(" << packetId << "): " << latency << std::endl;
}

int
main(int argc, char* argv[])
{
    double simulationTime{10}; // seconds
    std::size_t nStations{4};
    uint32_t payloadSize = 50; // must fit in the max TX duration when transmitting at MCS 0 over an RU of 26 tones
    double roomWidth{10.0}; // width of the room
    double roomHeight{10.0}; // height of the room

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    //cmd.AddValue("nStations", "Number of non-AP HE stations", nStations);
    cmd.AddValue("payloadSize", "The application payload size in bytes", payloadSize);
    cmd.AddValue("roomWidth", "The width of the room (2D)", roomWidth);
    cmd.AddValue("roomHeight", "The height of the room (2D)", roomHeight);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(nStations);

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;
    WifiMacHelper mac;
    WifiHelper wifi;

    Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue(40));

    wifi.SetStandard(WIFI_STANDARD_80211be);
    
    for (int i = 0;i < nStations;i++) 
    {
        SpectrumWifiPhyHelper phy(1);
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssids[i]));
        phy.Set(0, "ChannelSettings", StringValue("{1, 20, BAND_2_4GHZ, 0}"));

        Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
        Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel>();
        spectrumChannel->AddPropagationLossModel(lossModel);
        phy.AddChannel(spectrumChannel, WIFI_SPECTRUM_2_4_GHZ);
        staDevices.Add(wifi.Install(phy, mac, wifiStaNodes.Get(i)));

        mac.SetType("ns3::ApWifiMac",
                    "EnableBeaconJitter",
                    BooleanValue(false),
                    "Ssid",
                    SsidValue(ssids[i]));
        apDevices.Add(wifi.Install(phy, mac, wifiApNodes.Get(i)));
        
        phy.EnablePcapAll("test", true);
    }

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
    Ipv4InterfaceContainer staNodeInterfaces;
    Ipv4InterfaceContainer apNodeInterfaces;

    for (uint8_t i = 0; i < nStations;i++)
    {
        Ipv4AddressHelper address;
        Ipv4Address network(192 << 24 | 168 << 16 | i << 8 | 0);
        address.SetBase(network, "255.255.255.0");

        staNodeInterfaces.Add(address.Assign(staDevices.Get(i)));
        apNodeInterfaces.Add(address.Assign(apDevices.Get(i)));
    }


    /* Setting applications */
    // UDP flow
    uint16_t port = 8080;
    ApplicationContainer clientApplications;
    ApplicationContainer serverApplications;

    for (std::size_t i = 0; i < nStations; i++)
    {
        Ptr<TestApplication> client = CreateObject<TestApplication>(i);
        NS_LOG_DEBUG("[" << i << "]" << apNodeInterfaces.GetAddress(i) << ":" << port);
        client->SetAttribute("PeerAddress", AddressValue(apNodeInterfaces.GetAddress(i)));
        client->SetAttribute("PeerPort", UintegerValue(port));
        wifiStaNodes.Get(i)->AddApplication(client);
        clientApplications.Add(client);

        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(apNodeInterfaces.GetAddress(i), port));
        serverApplications.Add(sinkHelper.Install(wifiApNodes.Get(i)));
    }

    clientApplications.Start(Seconds(1.0));
    clientApplications.Stop(Seconds(simulationTime + 1));
    serverApplications.Start(Seconds(0.0));
    serverApplications.Stop(Seconds(simulationTime + 1));

    Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$TestApplication/RxLatency", MakeCallback(&RxLatency));

    Packet::EnablePrinting();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}

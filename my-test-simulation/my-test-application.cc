/*
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
 */

#include "my-test-application.h"

#include "ns3/applications-module.h"
#include "ns3/wifi-mac.h"
#include "ns3/qos-txop.h"
#include "ns3/wifi-mac-queue.h"
#include "ns3/wifi-phy.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TestApplication");

NS_OBJECT_ENSURE_REGISTERED(TestApplication);

TestApplication::TestApplication()
    : m_lmacs(),
      m_peerAddress(),
      m_peerPort(8000),
      m_interval(Seconds(0.5)),
      m_packetSize(50),
      m_nPackets(0),
      m_sendEvent(),
      m_packetsSent(0)
{
}

TestApplication::~TestApplication()
{
    for (auto p : m_lmacs)
    {
        p.socket = nullptr;
    }
}

/* static */
TypeId
TestApplication::GetTypeId()
{
    static TypeId tid = TypeId("TestApplication")
                            .SetParent<Application>()
                            .SetGroupName("Test")
                            .AddConstructor<TestApplication>()
                            .AddAttribute("PeerAddress",
                                          "The peer address",
                                          AddressValue(),
                                          MakeAddressAccessor(&TestApplication::m_peerAddress),
                                          MakeAddressChecker())
                            .AddAttribute("PeerPort",
                                          "The peer port number",
                                          UintegerValue(9),
                                          MakeUintegerAccessor(&TestApplication::m_peerPort),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("PacketSize",
                                          "The size of the packets in bytes",
                                          UintegerValue(50),
                                          MakeUintegerAccessor(&TestApplication::m_packetSize),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("NumberOfPackets",
                                          "The number of packets",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&TestApplication::m_nPackets),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("BurstSize",
                                          "The number of puckets sent every burst",
                                          UintegerValue(1),
                                          MakeUintegerAccessor(&TestApplication::m_burstSize),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("Interval",
                                          "The time interval between packets",
                                          TimeValue(Seconds(0.5)),
                                          MakeTimeAccessor(&TestApplication::m_interval),
                                          MakeTimeChecker(Seconds(0.0)))
                            .AddTraceSource("RxLatency",
                                            "Called when a packet is received",
                                            MakeTraceSourceAccessor(&TestApplication::m_receivedCallback),
                                            "TestApplication::LatencyCallback");
    return tid;
}

void
TestApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);
    m_packetsSent = 0;
    m_id = GetNode()->GetId();
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    for (uint32_t i = 0;i < GetNode()->GetNDevices();i++)
    {
        if (GetNode()->GetDevice(i)->GetInstanceTypeId() != WifiNetDevice::GetTypeId()) 
        {
            continue;
        }
        Ptr<WifiNetDevice> d = GetNode()->GetDevice(i)->GetObject<WifiNetDevice>();
        // Ptr<StaWifiMac> mac = d->GetMac()->GetObject<StaWifiMac>();
        // uint32_t bk_packets = mac->GetQosTxop(AcIndex::AC_BK)->GetWifiMacQueue()->GetNPackets();
        //mac->GetTxop()->GetWifiMacQueue()->GetNPackets();
        // Ptr<EhtFrameExchangeManager> fem = mac->GetFrameExchangeManager(0)->GetObject<EhtFrameExchangeManager>();
        NS_LOG_INFO("TestApplication[" << m_id << "]: MAC=" << Mac48Address::ConvertFrom(GetNode()->GetDevice(i)->GetAddress()));
        ns3::Ptr<ns3::Socket> m_socket = Socket::CreateSocket(GetNode(), tid);
        m_socket->BindToNetDevice(GetNode()->GetDevice(i));
        if (Ipv4Address::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort)) < 0)
            {
                NS_LOG_ERROR("TestApplication[" << m_id << "]: Error connecting socket (" << m_socket->GetErrno() << ")");
            }
        }
        else if (Ipv6Address::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Connect(Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort)) < 0)
            {
                NS_LOG_ERROR("TestApplication[" << m_id << "]: Error connecting socket (" << m_socket->GetErrno() << ")");
            }
        }
        else if (InetSocketAddress::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Connect(m_peerAddress) < 0)
            {
                NS_LOG_ERROR("TestApplication[" << m_id << "]: Error connecting socket (" << m_socket->GetErrno() << ")");
            }
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Connect(m_peerAddress) < 0)
            {
                NS_LOG_ERROR("TestApplication[" << m_id << "]: Error connecting socket (" << m_socket->GetErrno() << ")");
            }
        }
        else
        {
            NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
        }
        LowerMac lmac = LowerMac{
            d,
            m_socket,
        };
        m_lmacs.push_back(lmac);
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(GetNode()->GetId()) + "/DeviceList/" + std::to_string(i) + "/Mac/AckedMpdu", MakeCallback(&TestApplication::ReceiveAck, this, i));
        //Config::ConnectWithoutContext("/NodeList/" + std::to_string(GetNode()->GetId()) + "/DeviceList/" + std::to_string(i) + "/Mac/NAckedMpdu", MakeCallback(&TestApplication::ReceiveNAck, this));
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(GetNode()->GetId()) + "/DeviceList/" + std::to_string(i) + "/Mac/MpduResponseTimeout", MakeCallback(&TestApplication::Timeout, this));
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(GetNode()->GetId()) + "/DeviceList/" + std::to_string(i) + "/Phy/MonitorSnifferRx", MakeCallback(&TestApplication::MonitorRx, this, i));
    }
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &TestApplication::SendPacket, this);
}

void
TestApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }
}

void
TestApplication::SendPacket()
{
    NS_LOG_FUNCTION(this);
    for (auto lmac : m_lmacs)
    {
        SeqTsHeader seqTs;
        seqTs.SetSeq(m_packetsSent);
        Ptr<Packet> packet = Create<Packet>(m_packetSize - seqTs.GetSerializedSize() - 28); // Rimuovo header IP e UDP
        NS_LOG_DEBUG("TestApplication[" << m_id << "]: Send Packet (" << m_packetsSent << ") to " << Ipv4Address::ConvertFrom(m_peerAddress));
        packet->AddHeader(seqTs);
        if (lmac.socket->Send(packet) < 0)
        {
            NS_LOG_ERROR("TestApplication[" << m_id << "]Error sending packet: errno " << lmac.socket->GetErrno());
        }
    }
    ++m_packetsSent;

    if (m_packetsSent % m_burstSize == 0)
    {
        if (m_packetsSent < m_nPackets || m_nPackets == 0)
        {
            m_sendEvent = Simulator::Schedule(m_interval, &TestApplication::SendPacket, this);
        }
    }
    else
    {
        m_sendEvent = Simulator::Schedule(MicroSeconds(5), &TestApplication::SendPacket, this);
    }

}

uint32_t
GetNPackets(Ptr<WifiNetDevice> device, AcIndex ac)
{
    return device->GetMac()->GetQosTxop(ac)->GetWifiMacQueue()->GetNPackets();
}

void
TestApplication::ReceiveAck(uint32_t i, ns3::Ptr<const ns3::WifiMpdu> mpdu)
{
    NS_LOG_FUNCTION(this);
    LowerMac &lmac = m_lmacs.at(i);
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;
    if (p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader)) 
    {
        uint32_t be_packets = GetNPackets(lmac.device, AcIndex::AC_BE);
        m_receivedCallback({
            m_id,
            lmac.device->GetPhy(0)->GetChannelNumber(),
            lmac.device->GetPhy(0)->GetChannelWidth(),
            seqTsHeader.GetSeq(),
            Simulator::Now() - seqTsHeader.GetTs(),
            be_packets,
            lmac.signalNoise
        });
    }
    else
    {
        NS_LOG_WARN("TestApplication[" << m_id << "]: Received unkonwn packet " << mpdu);
    }
}

void
TestApplication::ReceiveNAck(ns3::Ptr<const ns3::WifiMpdu> mpdu)
{
    NS_LOG_FUNCTION(this);
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;
    if (p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader)) 
    {
        if (seqTsHeader.GetSeq() <= m_packetsSent) 
        {
            //std::cout << "[" << m_id << "]NACK(" << seqTsHeader.GetSeq() << ")" << std::endl;
        }
    }
    else
    {
        NS_LOG_WARN("TestApplication[" << m_id << "]: NACK unkonwn packet " << mpdu);
    }
}

void
TestApplication::Timeout(uint8_t reason, ns3::Ptr<const ns3::WifiMpdu> mpdu, const WifiTxVector& txVector)
{
    NS_LOG_FUNCTION(this);
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;
    if (mpdu->GetPacketSize() > 0 && p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader)) 
    {
        NS_LOG_ERROR("TestApplication[" << m_id << "]: TIMEOUT(" << seqTsHeader.GetSeq() << ") " << txVector);
    }
    else
    {
        NS_LOG_WARN("TestApplication[" << m_id << "]: Timeout unkonwn packet " << mpdu);
    }
}

void 
TestApplication::MonitorRx(uint32_t i, Ptr<const Packet> packet,
                                             uint16_t channelFreqMhz,
                                             WifiTxVector txVector,
                                             MpduInfo aMpdu,
                                             SignalNoiseDbm signalNoise,
                                             uint16_t staId)
{
    NS_LOG_FUNCTION(this);
    LowerMac &lmac = m_lmacs.at(i);
    Ptr<Packet> p = packet->Copy();
    WifiMacHeader macHeader;
    if (p->RemoveHeader(macHeader) && (macHeader.IsAck() || macHeader.IsBlockAck()) && macHeader.GetAddr1() == Mac48Address::ConvertFrom(lmac.device->GetAddress())) 
    {
        lmac.signalNoise = signalNoise;
    }
    else
    {
        NS_LOG_WARN("TestApplication[" << m_id << "]: Received unkonwn packet " << packet);
    }
}

#include "sta-logger.h"

#include "ns3/ipv4-l3-protocol.h"
#include "ns3/llc-snap-header.h"
#include "ns3/seq-ts-header.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/udp-header.h"
#include "ns3/wifi-psdu.h"

NS_LOG_COMPONENT_DEFINE("StaLogger");

STALogger::STALogger(std::string out_file_path, Arguments args): _args(args)
{
    AsciiTraceHelper asciiHelper;
    _output_file.open(out_file_path.c_str(), std::ios::out | std::ios::trunc);
}

void STALogger::logHeader()
{
    _output_file << "[" << std::endl << json(_args);
}

void STALogger::logFooter(std::chrono::seconds duration)
{
    _output_file << "," << std::endl << json({
        {"elapsed_seconds",  duration.count()}
    }) << std::endl << "]";
}

void STALogger::sendingMpduCallback(WifiConstPsduMap psduMap, WifiTxVector txVector, double txPowerW)
{   
    NS_LOG_DEBUG("Psdu map size:" << psduMap.size());
    NS_LOG_DEBUG("Tracked packet number:" << _packets.size());

    for (auto& it: psduMap)
    {
        const WifiPsdu& psdu =  *it.second;

        NS_LOG_DEBUG("Mdpu in psdu: " << psdu.GetNMpdus());
        
        for (auto it: psdu)
        {
            const WifiMpdu& mpdu = *it;
            Ptr<Packet> p = mpdu.GetPacket()->Copy(); 
            LlcSnapHeader llcSnapHeader;
            Ipv4Header ipv4Header;
            UdpHeader udpHeader;
            SeqTsHeader seqTsHeader;    
            if (p->GetSize() > 0 && p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
            {
                const auto it = _packets.find(seqTsHeader.GetSeq());
                PacketInfo info {seqTsHeader.GetSeq()};
                if (it != _packets.end())
                {
                    info = it->second;
                    if (info.rate != 0)
                    {
                        NS_FATAL_ERROR("Retransmission of packet before timeout");
                    }
                }
                info.rate = txVector.GetMode().GetDataRate(txVector);
                _packets.insert_or_assign(seqTsHeader.GetSeq(), info);
            }
        }
    }
}

void STALogger::ackedMpduCallback(Ptr<const WifiMpdu> mpdu)
{
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;    
    if (p->GetSize() > 0 && p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
    {
        PacketInfo info {seqTsHeader.GetSeq()};
        const auto it = _packets.find(seqTsHeader.GetSeq());
        if (it != _packets.end())
        {
            info = it->second;
        }
        else
        {
            NS_FATAL_ERROR("Acked packet was not sent");
        }
        info.acked = true;
        info.latency = Simulator::Now() - seqTsHeader.GetTs();
        _output_file << ", " << std::endl << json(info);
        _packets.erase(it, _packets.end());
    }
}

// Callback for retransmission
void STALogger::mpduTimeoutCallback(uint8_t reason, Ptr<const WifiMpdu> mpdu, const WifiTxVector& tx_vector)
{
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;
    if (p->GetSize() > 0 && p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
    {
        PacketInfo info {seqTsHeader.GetSeq()};
        auto it = _packets.find(seqTsHeader.GetSeq());
        if (it != _packets.end())
        {
            info = it->second;
        }
        else
        {
            NS_FATAL_ERROR("Timeout packet was not sent");
        }
        uint64_t rate = tx_vector.GetMode().GetDataRate(tx_vector);
        if (rate!= info.rate)
        {
            NS_FATAL_ERROR("Rate mismatch on sent packet");
        }
        info.retransmissions.push_back(RetransmissionInfo
        {
            rate,
            Simulator::Now() - seqTsHeader.GetTs()
        });
        info.rate = 0;
        _packets.insert_or_assign(seqTsHeader.GetSeq(), info);
        
    }
}

// Callback for dropped packet
void STALogger::droppedMpduCallback(WifiMacDropReason reason, Ptr<const WifiMpdu> mpdu)
{
    Ptr<Packet> p = mpdu->GetPacket()->Copy();
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;
    if (p->GetSize() > 0 && p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
    {
        PacketInfo info {seqTsHeader.GetSeq()};
        const auto it = _packets.find(seqTsHeader.GetSeq());
        if (it != _packets.end())
        {
            info = it->second;
        }
        else
        {
            NS_FATAL_ERROR("Dropped packet was not sent");
        }
        info.acked = false;
        info.latency = Simulator::Now() - seqTsHeader.GetTs();
        _output_file << ", " << std::endl << json(info);
        _packets.erase(it, _packets.end());
    }
}

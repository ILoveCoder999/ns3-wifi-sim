#include "sta-logger.h"

#include "ns3/ipv4-l3-protocol.h"
#include "ns3/llc-snap-header.h"
#include "ns3/seq-ts-header.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/udp-header.h"
#include "ns3/wifi-psdu.h"
#include "ns3/wifi-net-device.h"

NS_LOG_COMPONENT_DEFINE("StaLogger");

STALogger::STALogger(std::string out_file_path, std::string header, Ptr<WifiNetDevice> net_dev, Ptr<MobilityModel> mobility):
     _header(header), _net_dev(net_dev), _mobility(mobility)
{
    _output_file.open(out_file_path.c_str(), std::ios::out | std::ios::trunc);
}

void STALogger::logHeader()
{
    _output_file << "[" << std::endl << _header;
}

void STALogger::logFooter(std::chrono::seconds duration)
{
    _output_file << "," << std::endl << json({
        {"elapsed_seconds",  duration.count()}
    }) << std::endl << "]";
}

void STALogger::sendingMpduCallback(WifiConstPsduMap psduMap, WifiTxVector txVector, double txPowerW)
{   
    if (psduMap.size() > 1)
    {
        NS_LOG_DEBUG("Psdu map size:" << psduMap.size());
    }
    if (_packets.size() > 1)
    {
        NS_LOG_DEBUG("Tracked packet number:" << _packets.size());
    }    

    for (auto& it_psdu: psduMap)
    {
        Ptr<const WifiPsdu> psdu =  it_psdu.second;
        if (psdu->GetNMpdus() > 1)
        {
            NS_LOG_DEBUG("Mdpu in psdu: " << psdu->GetNMpdus());
        }        
        for (auto it_mpdu: *psdu)
        {
            const WifiMpdu& mpdu = *it_mpdu;
            Ptr<Packet> p = mpdu.GetPacket()->Copy(); 
            LlcSnapHeader llcSnapHeader;
            Ipv4Header ipv4Header;
            UdpHeader udpHeader;
            SeqTsHeader seqTsHeader;
            if (p->GetSize() > 0 && p->RemoveHeader(llcSnapHeader) && p->RemoveHeader(ipv4Header) && p->RemoveHeader(udpHeader) && p->RemoveHeader(seqTsHeader))
            {
                const auto it = _packets.find(seqTsHeader.GetSeq());
                PacketInfo info {seqTsHeader.GetSeq()};
                
                // uint32_t payload_size = p->GetSize() + seqTsHeader.GetSerializedSize();
                // if (payload_size != _args.staNode.payloadSize) {
                //     NS_FATAL_ERROR("Mismatching payload size: original " << _args.staNode.payloadSize << ", obtained " << payload_size);
                // }

                if (it != _packets.end())
                {
                    info = it->second;
                    if (info.current_tx)
                    {
                        NS_FATAL_ERROR("Transmission still pending");
                    }
                }
                std::ostringstream os;
                os << mpdu.GetHeader().GetAddr1();
                info.addr_1 = os.str();
                info.current_tx = std::make_shared<TransmissionInfo>();
                info.current_tx->rate = txVector.GetMode().GetDataRate(txVector);
                info.current_tx->tx_power_w = txPowerW;
                info.current_tx->tx_duration = WifiPhy::CalculateTxDuration(psdu, txVector, _net_dev->GetPhy()->GetPhyBand());
                Vector position = _mobility->GetPosition();
                info.current_tx->position = std::make_tuple(position.x, position.y, position.z);
                info.current_tx->tx_time = Simulator::Now();
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
        Time latency = Simulator::Now() - seqTsHeader.GetTs();
        info.acked = true;
        info.latency = latency;
        info.current_tx->latency = latency;
        info.transmissions.push_back(*info.current_tx);
        info.current_tx = nullptr;
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
        if (info.current_tx->rate != tx_vector.GetMode().GetDataRate(tx_vector)) // THIS CHECK CAN BE REMOVED AFTER DEBUG
        {
            NS_FATAL_ERROR("Rate mismatch on sent packet");
        }
        info.current_tx->latency = Simulator::Now() - seqTsHeader.GetTs();
        info.transmissions.push_back(*info.current_tx);
        info.current_tx = nullptr;
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
        Time latency = Simulator::Now() - seqTsHeader.GetTs();
        info.acked = false;
        info.latency = latency;
        if (info.current_tx)
        {
            NS_LOG_DEBUG("Dropping transmission without timeout");
            info.current_tx->latency = latency;
            info.transmissions.push_back(*info.current_tx);
            info.current_tx = nullptr;
        }        
        _output_file << ", " << std::endl << json(info);
        _packets.erase(it, _packets.end());
    }
}

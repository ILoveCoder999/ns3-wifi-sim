//
// Created by matteo on 26/08/24.
//

#include "my-packets-error-model.h"

#include "ns3/object-factory.h"
#include "ns3/pointer.h"
#include "ns3/enum.h"
#include "ns3/double.h"
#include "ns3/ipv4-header.h"
#include "ns3/llc-snap-header.h"
#include "ns3/packet.h"
#include "ns3/seq-ts-header.h"
#include "ns3/udp-header.h"
#include "ns3/wifi-mac-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MyPacketsErrorModel");

NS_OBJECT_ENSURE_REGISTERED(MyPacketsErrorModel);

MyPacketsErrorModel::MyPacketsErrorModel()
{
    NS_LOG_FUNCTION(this);
}

MyPacketsErrorModel::~MyPacketsErrorModel()
{
    NS_LOG_FUNCTION(this);
}

TypeId
MyPacketsErrorModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::MyPacketsRateErrorModel")
            .SetParent<ErrorModel>()
            .SetGroupName("Network")
            .AddConstructor<MyPacketsErrorModel>()
            .AddAttribute("ErrorModel",
                          "The inner error model to apply only to the filtered packets",
                          PointerValue(CreateObjectWithAttributes<RateErrorModel>(
                              "ErrorUnit", EnumValue(RateErrorModel::ErrorUnit::ERROR_UNIT_PACKET),
                              "ErrorRate", DoubleValue(0.2))),
                          MakePointerAccessor(&MyPacketsErrorModel::m_errorModel),
                          MakePointerChecker<ErrorModel>());
    return tid;
}

bool
MyPacketsErrorModel::DoCorrupt(Ptr<Packet> pkt)
{
    Ptr<Packet> p = pkt->Copy();
    WifiMacHeader wifiMacHeader;
    LlcSnapHeader llcSnapHeader;
    Ipv4Header ipv4Header;
    UdpHeader udpHeader;
    SeqTsHeader seqTsHeader;

    if (pkt->GetSize() > 0 &&
        pkt->RemoveHeader(wifiMacHeader) && (wifiMacHeader.GetType() == WIFI_MAC_QOSDATA || wifiMacHeader.GetType() == WIFI_MAC_DATA) &&
        pkt->RemoveHeader(llcSnapHeader) && llcSnapHeader.GetType() == 0x800 &&
        pkt->RemoveHeader(ipv4Header) && ipv4Header.GetProtocol() == 17 &&
        pkt->RemoveHeader(udpHeader) && udpHeader.GetDestinationPort() == 9 &&
        pkt->RemoveHeader(seqTsHeader))
    {
        return m_errorModel->IsCorrupt(p);
    }
    return false;
}

void
MyPacketsErrorModel::DoReset()
{
    m_errorModel->Reset();
}


} // ns3
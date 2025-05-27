#ifndef STA_LOGGER_H
#define STA_LOGGER_H

// standard includes
#include <fstream>

// ns3 includes
#include "ns3/pointer.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-mpdu.h"
#include "ns3/he-frame-exchange-manager.h"
#include "ns3/mobility-model.h"

// custom
#include "packet-info.h"

using namespace ns3;

class STALogger
{
    public:
        STALogger(std::string out_file_path, std::string header, Ptr<WifiNetDevice> net_dev, Ptr<MobilityModel> mobility);
        ~STALogger() {};

        void logHeader();
        void logFooter(std::chrono::seconds);

        /* CALLBACKS
        - Trace sources in WifiMac, called from src/wifi/model/frame-exchange-manager.cc (see SendMpdu() function)
        - Only mdpuTimeoutCallback can access txVector (it is the one that was used to send the packet)
        */
        void ackedMpduCallback(Ptr<const WifiMpdu> mpdu);
        void mpduTimeoutCallback(uint8_t reason, Ptr<const WifiMpdu> mpdu, const WifiTxVector& tx_vector);
        void droppedMpduCallback(WifiMacDropReason reason, Ptr<const WifiMpdu> mpdu);
        /*
        Callback for obtaining data rate, transmission time and tx power
        - to have tx power changes multiple power levels should be configured via WifiPhy
        - this also requires a rate control algorithms that tunes tx power (e.g. ns3::ParfWifiManager)
        */
       void sendingMpduCallback(WifiConstPsduMap psduMap, WifiTxVector txVector, double txPowerW); //PhyTxPsduBegin
    
    protected:
        std::ofstream _output_file;
        std::string _header;       
        Ptr<WifiNetDevice> _net_dev;
        Ptr<MobilityModel> _mobility;
        std::unordered_map<uint32_t, PacketInfo> _packets;        
};

#endif
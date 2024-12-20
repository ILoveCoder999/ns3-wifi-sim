#ifndef STA_LOGGER_H
#define STA_LOGGER_H

// standard includes
#include <fstream>

// ns3 includes
#include "ns3/pointer.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-mpdu.h"
#include "ns3/he-frame-exchange-manager.h"

// custom
#include "arguments.h"
#include "packet-info.h"

using namespace ns3;

class STALogger
{
    public:
        STALogger(std::string out_file_path, Arguments args, Ptr<WifiNetDevice> net_dev);
        virtual ~STALogger() {};

        void logHeader();
        virtual void logFooter(std::chrono::seconds);

        /* CALLBACKS
        - Trace sources in WifiMac, called from src/wifi/model/frame-exchange-manager.cc (see SendMpdu() function)
        - Only mdpuTimeoutCallback can access txVector (it is the one that was used to send the packet)
        - TODO: read data rate from sent packet (physical layer callback, the mpdu must be searched in the psdu)
            https://www.nsnam.org/docs/release/3.43/doxygen/dc/d2d/classns3_1_1_wifi_phy.html#a570e0119c2b01f5be064d7d49c598a4a
        */
        void sendingMpduCallback(WifiConstPsduMap psduMap, WifiTxVector txVector, double txPowerW); //PhyTxPsduBegin
        void ackedMpduCallback(Ptr<const WifiMpdu> mpdu);
        void mpduTimeoutCallback(uint8_t reason, Ptr<const WifiMpdu> mpdu, const WifiTxVector& tx_vector);  
        void droppedMpduCallback(WifiMacDropReason reason, Ptr<const WifiMpdu> mpdu);
    
    protected:
        std::ofstream _output_file;
        Ptr<WifiNetDevice> _net_dev;

    private:
        Arguments _args;
        std::unordered_map<uint32_t, PacketInfo> _packets;
};

#endif
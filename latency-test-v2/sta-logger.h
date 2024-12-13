#ifndef STA_LOGGER_H
#define STA_LOGGER_H

// standard includes
#include <fstream>

// ns3 includes
#include "ns3/pointer.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-mpdu.h"

// custom
#include "arguments.h"
#include "packet-info.h"

using namespace ns3;

class STALogger
{
    public:
        STALogger(std::string out_file_path, Arguments args);

        void logHeader();
        virtual void logFooter(std::chrono::seconds);

        // callbacks
        void ackedMpduCallback(Ptr<const WifiMpdu> mpdu);
        void mpduTimeoutCallback(uint8_t reason, Ptr<const WifiMpdu> mpdu, const WifiTxVector& tx_vector);
        void droppedMpduCallback(WifiMacDropReason reason, Ptr<const WifiMpdu> mpdu);
    
    protected:
        std::ofstream _output_file;

    private:
        Arguments _args;
        std::unordered_map<uint32_t, PacketInfo> _packets;
};

#endif
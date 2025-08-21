#ifndef ROAMING_MANAGER_H
#define ROAMING_MANAGER_H

// ns3 includes
#include "ns3/pointer.h"
#include "ns3/mobility-model.h"
#include "ns3/wifi-mac.h"
#include "ns3/sta-wifi-mac.h"

using namespace ns3;

class RoamingManager
{
    public:
        RoamingManager(Ptr<WifiNetDevice> staDevice, Ptr<MobilityModel> mobility, std::vector<uint8_t> channel_list);
        ~RoamingManager() {};

        /* CALLBACKS*/
        void assocCallback(Mac48Address value);
        void deAssocCallback(Mac48Address value);
        void receivedBeaconInfoCallback(StaWifiMac::ApInfo apInfo);
    
    private:
        std::tuple<double, double, double> _currentPosition();
        void _switch_channel();

        Ptr<WifiNetDevice> _staDevice;
        Ptr<MobilityModel> _mobility;
        std::vector<uint8_t> _channel_list;
        size_t _cur_channel_idx;
        std::string _band;
        uint16_t _channel_width;
        uint8_t _primary_20_idx;
};

#endif

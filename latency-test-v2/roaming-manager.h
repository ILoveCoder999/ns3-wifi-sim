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
        RoamingManager(Ptr<WifiNetDevice> staDevice, Ptr<MobilityModel> mobility, std::vector<std::string> channel_list);
        ~RoamingManager() {};

        /* CALLBACKS*/
        void assocCallback(Mac48Address value);
        void deAssocCallback(Mac48Address value);
        void receivedBeaconInfoCallback(StaWifiMac::ApInfo apInfo);

        /* HELPERS*/
        static std::string extractChannelString(Ptr<WifiNetDevice> staDevice);
    
    private:
        std::tuple<double, double, double> _currentPosition();
        void _switch_channel();

        Ptr<WifiNetDevice> _staDevice;
        Ptr<MobilityModel> _mobility;
        std::vector<std::string> _channel_list;
        size_t _cur_channel_idx;
};

#endif

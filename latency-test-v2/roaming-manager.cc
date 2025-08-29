#include "roaming-manager.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h"
#include "ns3/string.h"

NS_LOG_COMPONENT_DEFINE("RoamingManager");

RoamingManager::RoamingManager(Ptr<WifiNetDevice> staDevice, Ptr<MobilityModel> mobility, std::vector<std::string> channel_list):
     _staDevice(staDevice), _mobility(mobility), _channel_list(channel_list)
{
    std::string curr_channel = RoamingManager::extractChannelString(_staDevice);
    std::vector<std::string>::iterator iter = std::find(_channel_list.begin(), _channel_list.end(), curr_channel);
    size_t index = std::distance(_channel_list.begin(), iter);
    if (index == _channel_list.size()) {
        _channel_list.insert(_channel_list.begin(), curr_channel);
        index = 0;
    }
    _cur_channel_idx = index;
}

void RoamingManager::assocCallback(Mac48Address value) {}

void RoamingManager::deAssocCallback(Mac48Address value)
{
    _switch_channel();
}

void RoamingManager::receivedBeaconInfoCallback(StaWifiMac::ApInfo apInfo) {}

std::string RoamingManager::extractChannelString(Ptr<WifiNetDevice> staDevice)
{
    Ptr<WifiPhy> phy = staDevice->GetPhy();
    std::string band;
    std::stringstream ss;

    ss << phy->GetPhyBand();
    band = ss.str();
    std::transform(band.begin(), band.end(), band.begin(),::toupper);
    
    ss.str(std::string());
    ss << "{"
        << std::to_string(phy->GetChannelNumber()) << ","
        << std::to_string(phy->GetChannelWidth()) << ","
        << "BAND_" << band
        << "," << std::to_string(phy->GetPrimary20Index()) << "}";
    NS_LOG_FUNCTION(ss.str());
    return ss.str();
}

inline void RoamingManager::_switch_channel()
{
    _cur_channel_idx = (_cur_channel_idx + 1) % _channel_list.size();
    _staDevice->GetPhy()->SetAttribute("ChannelSettings", StringValue(_channel_list[_cur_channel_idx]));
    NS_LOG_FUNCTION(_channel_list[_cur_channel_idx]);
}

inline std::tuple<double, double, double> RoamingManager::_currentPosition()
{
    Vector position = _mobility->GetPosition();
    return std::make_tuple(position.x, position.y, position.z);
}

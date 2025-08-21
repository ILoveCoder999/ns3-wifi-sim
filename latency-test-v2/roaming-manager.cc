#include "roaming-manager.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h"
#include "ns3/string.h"

NS_LOG_COMPONENT_DEFINE("RoamingManager");

RoamingManager::RoamingManager(Ptr<WifiNetDevice> staDevice, Ptr<MobilityModel> mobility, std::vector<uint8_t> channel_list):
     _staDevice(staDevice), _mobility(mobility), _channel_list(channel_list)
{
    Ptr<WifiPhy> phy = _staDevice->GetPhy();
    uint8_t ch_num = phy->GetChannelNumber();
    std::vector<uint8_t>::iterator iter = std::find(_channel_list.begin(), _channel_list.end(), ch_num);
    size_t index = std::distance(_channel_list.begin(), iter);
    if (index == _channel_list.size()) {
        _channel_list.insert(_channel_list.begin(), ch_num);
        index = 0;
    }
    _cur_channel_idx = index;
    _channel_width = phy->GetChannelWidth();
    std::stringstream ss;
    ss << phy->GetPhyBand();
    _band = ss.str();
    std::transform(_band.begin(), _band.end(), _band.begin(),::toupper);
    _primary_20_idx = phy->GetPrimary20Index();
}

void RoamingManager::assocCallback(Mac48Address value) {}

void RoamingManager::deAssocCallback(Mac48Address value)
{
    _switch_channel();
}

void RoamingManager::receivedBeaconInfoCallback(StaWifiMac::ApInfo apInfo) {}

inline void RoamingManager::_switch_channel()
{
    std::stringstream ss;
    _cur_channel_idx = (_cur_channel_idx + 1) % _channel_list.size();
    ss << "{"
        << std::to_string(_channel_list[_cur_channel_idx]) << ", "
        << std::to_string(_channel_width) << ", "
        << "BAND_" << _band
        << ", " << "0" << "}";
    NS_LOG_FUNCTION(ss.str());
    _staDevice->GetPhy()->SetAttribute("ChannelSettings", StringValue(ss.str()));
}

inline std::tuple<double, double, double> RoamingManager::_currentPosition()
{
    Vector position = _mobility->GetPosition();
    return std::make_tuple(position.x, position.y, position.z);
}

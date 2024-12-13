#include "sta-logger-power.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/wifi-net-device.h"


STALoggerPower::STALoggerPower(std::string out_file_path, Arguments args, Ptr<WifiNetDevice> net_dev, uint32_t packet_size):
    STALogger(out_file_path, args), _current_power(0), _total_energy(0), _total_time(0), _net_dev(net_dev), _packet_size(packet_size)
{
    Ptr<WifiPhy> phy = _net_dev->GetPhy();
    _setupPhy(phy);
    _current_rate = DataRate(phy->GetDefaultMode().GetDataRate(phy->GetChannelWidth()));
    _current_power = phy->GetTxPowerEnd();
}

void STALoggerPower::logFooter(std::chrono::seconds duration)
{
    _output_file << "," << std::endl << json({
        {"elapsed_seconds",  duration.count()},
        {"avg_tx_power_mw", _total_energy / _total_time}
    }) << std::endl << "]";
}

void STALoggerPower::powerChangeCallback(std::string path, double old_power, double new_power, Mac48Address dest)
{
    _current_power = new_power;
}

void STALoggerPower::rateChangeCallback(std::string path, DataRate old_rate, DataRate new_rate, Mac48Address dest)
{
    _current_rate = new_rate;
}

void STALoggerPower::phyTxCallback(std::string path, Ptr<const Packet> packet, double power_w)
{
    WifiMacHeader head;
    packet->PeekHeader(head);

    if (head.GetType() == WIFI_MAC_DATA)
    {
        double tx_time = _getCurrTxTime().GetSeconds();
        _total_energy += pow(10.0, _current_power / 10.0) * tx_time;
        _total_time += tx_time;
    }

    _output_file << ", " << std::endl << json({
        {"current_power",  pow(10.0, _current_power / 10.0) / 1000},
        {"callback_power", power_w}
    });

}

void STALoggerPower::_setupPhy(Ptr<WifiPhy> phy)
{
    for (const auto& mode : phy->GetModeList())
    {
        WifiTxVector txVector;
        txVector.SetMode(mode);
        txVector.SetPreambleType(WIFI_PREAMBLE_LONG);
        txVector.SetChannelWidth(phy->GetChannelWidth());
        DataRate dataRate(mode.GetDataRate(phy->GetChannelWidth()));

        Time time = phy->CalculateTxDuration(_packet_size, txVector, phy->GetPhyBand());
        //NS_LOG_DEBUG(mode.GetUniqueName() << " " << time.GetSeconds() << " " << dataRate);
        _time_rate_table.emplace_back(time, dataRate);
    }
}

Time STALoggerPower::_getCurrTxTime()
{
    for (auto i = _time_rate_table.begin(); i != _time_rate_table.end(); i++)
    {
        if (_current_rate == i->second)
        {
            return i->first;
        }
    }
    NS_ASSERT(false);
    return Seconds(0);
}

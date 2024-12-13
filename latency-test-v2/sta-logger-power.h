#ifndef STA_LOGGER_POWER_H
#define STA_LOGGER_POWER_H

#include "sta-logger.h"

// written starting from "examples/wireless/wifi-power-adaptation-distance.cc"

class STALoggerPower: public STALogger 
{
    public:
        STALoggerPower(std::string out_file_path, Arguments args, Ptr<WifiNetDevice> _net_dev, uint32_t packet_size);

        void logFooter(std::chrono::seconds) override;

        void powerChangeCallback(std::string path, double old_power, double new_power, Mac48Address dest); // power is in dBm (10log_10(P/1mW)))
        void phyTxCallback(std::string path, Ptr<const Packet> packet, double power_w);                    // power is in Watt
        void rateChangeCallback(std::string path, DataRate old_rate, DataRate new_rate, Mac48Address dest);

    private:
        typedef std::vector<std::pair<Time, DataRate>> TxTime;

        double _current_power;
        DataRate _current_rate;
        double _total_energy;
        double _total_time;

        Ptr<WifiNetDevice> _net_dev;
        const uint32_t _packet_size;
        TxTime _time_rate_table;

        void _setupPhy(Ptr<WifiPhy> phy);
        Time _getCurrTxTime(); 
};

#endif
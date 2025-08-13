//
// Created by matteo on 23/07/24.
//

#ifndef PACKET_INFO_H
#define PACKET_INFO_H

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct TransmissionInfo
{
    uint64_t rate;
    ns3::Time latency;
    ns3::Time tx_duration;
    double tx_power_w;
    std::tuple<double, double, double> position;
    ns3::Time tx_time;
};

inline void to_json(json& j, const TransmissionInfo& r)
{
    j = json {
            {"rate", r.rate},
            {"latency", r.latency.ToInteger(ns3::Time::Unit::NS)},
            {"tx_duration", r.tx_duration.ToInteger(ns3::Time::Unit::NS)},
            {"tx_power_w", r.tx_power_w},
            {"position", r.position},
            {"tx_time", r.tx_time.ToInteger(ns3::Time::Unit::NS)},
    };
}

struct PacketInfo
{
    uint32_t seq;
    bool acked;
    ns3::Time latency;
    std::vector<TransmissionInfo> transmissions;
    std::shared_ptr<TransmissionInfo> current_tx;
    std::string addr_1 = "";
};

inline void to_json(json& j, const PacketInfo& p)
{
    j = json {
            {"seq", p.seq},
            {"acked", p.acked},
            {"latency", p.latency.ToInteger(ns3::Time::Unit::NS)},
            {"transmissions", p.transmissions},
            {"addr_1", p.addr_1}
    };
}

#endif //PACKET_INFO_H

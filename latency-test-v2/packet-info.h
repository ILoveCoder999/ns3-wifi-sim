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
    ns3::Time tx_time;
    double tx_power_w;
};

inline void to_json(json& j, const TransmissionInfo& r)
{
    j = json {
            {"rate", r.rate},
            {"latency", r.latency.ToInteger(ns3::Time::Unit::NS)},
            {"tx_time", r.tx_time.ToInteger(ns3::Time::Unit::NS)},
            {"tx_power_w", r.tx_power_w}
    };
}

struct PacketInfo
{
    uint32_t seq;
    bool acked;
    ns3::Time latency;
    std::vector<TransmissionInfo> transmissions;
    std::shared_ptr<TransmissionInfo> current_tx;
};

inline void to_json(json& j, const PacketInfo& p)
{
    j = json {
            {"seq", p.seq},
            {"acked", p.acked},
            {"latency", p.latency.ToInteger(ns3::Time::Unit::NS)},
            {"transmissions", p.transmissions}
    };
}

#endif //PACKET_INFO_H

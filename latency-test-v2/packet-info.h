//
// Created by matteo on 23/07/24.
//

#ifndef PACKET_INFO_H
#define PACKET_INFO_H

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct RetransmissionInfo
{
    uint64_t rate;
    ns3::Time latency;
};

inline void to_json(json& j, const RetransmissionInfo& r)
{
    j = json {
            {"rate", r.rate},
            {"latency", r.latency.ToInteger(ns3::Time::Unit::NS)}
    };
}

struct PacketInfo
{
    uint32_t seq;
    bool acked;
    uint64_t rate;
    ns3::Time latency;
    std::vector<RetransmissionInfo> retransmissions;
};

inline void to_json(json& j, const PacketInfo& p)
{
    j = json {
            {"seq", p.seq},
            {"acked", p.acked},
            {"rate", p.rate},
            {"latency", p.latency.ToInteger(ns3::Time::Unit::NS)},
            {"retransmissions", p.retransmissions}
    };
}

#endif //PACKET_INFO_H

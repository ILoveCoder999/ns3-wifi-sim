//
// Created by matteo on 04/07/24.
//

#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define JSON_GET_OR_DEFAULT(j, key, def, dest) if (j.contains(#key)) { j[#key].get_to(dest.key); } else { dest.key = def.key; }

struct Position
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

inline void from_json(const json& j, Position& p)
{
  constexpr Position def{};
  JSON_GET_OR_DEFAULT(j, x, def, p);
  JSON_GET_OR_DEFAULT(j, y, def, p);
  JSON_GET_OR_DEFAULT(j, z, def, p);
}

struct InterferentConfig
{
  Position position{};
  std::string offTime = "ns3::ExponentialRandomVariable[Mean=0.25|Bound=10]";
  std::string burstSize = "ns3::ExponentialRandomVariable[Mean=100|Bound=500]";
  uint32_t rtsCtsThreshold = 4692480;
  std::string remoteStationManager = "ns3::MinstrelHtWifiManager";
  std::string dataMode = "OfdmRate6Mbps";
};

inline void from_json(const json& j, InterferentConfig& i)
{
  const InterferentConfig def{};
  JSON_GET_OR_DEFAULT(j, position, def, i);
  JSON_GET_OR_DEFAULT(j, offTime, def, i);
  JSON_GET_OR_DEFAULT(j, burstSize, def, i);
  JSON_GET_OR_DEFAULT(j, rtsCtsThreshold, def, i);
  JSON_GET_OR_DEFAULT(j, remoteStationManager, def, i);
  JSON_GET_OR_DEFAULT(j, dataMode, def, i);
}

struct StaConfig
{
  Position position{};
  uint32_t payloadSize = 50;
  double packetInterval = 0.5;
  uint32_t rtsCtsThreshold = 4692480;
  std::string remoteStationManager = "ns3::MinstrelHtWifiManager";
  std::string dataMode = "OfdmRate6Mbps";
};

inline void from_json(const json& j, StaConfig& s)
{
  const StaConfig def{};
  JSON_GET_OR_DEFAULT(j, position, def, s);
  JSON_GET_OR_DEFAULT(j, payloadSize, def, s);
  JSON_GET_OR_DEFAULT(j, packetInterval, def, s);
  JSON_GET_OR_DEFAULT(j, rtsCtsThreshold, def, s);
  JSON_GET_OR_DEFAULT(j, remoteStationManager, def, s);
  JSON_GET_OR_DEFAULT(j, dataMode, def, s);
}

struct Arguments
{
  StaConfig sta{};
  std::vector<InterferentConfig> interferent{};
  double simulationTime = 10;
  bool enablePcap = false;
  std::string pcapPrefix = "";
};

inline void from_json(const json& j, Arguments& a)
{
  const Arguments def{};
  JSON_GET_OR_DEFAULT(j, sta, def, a);
  JSON_GET_OR_DEFAULT(j, interferent, def, a);
  JSON_GET_OR_DEFAULT(j, simulationTime, def, a);
  JSON_GET_OR_DEFAULT(j, enablePcap, def, a);
  JSON_GET_OR_DEFAULT(j, pcapPrefix, def, a);
}

#endif //ARGUMENTS_H

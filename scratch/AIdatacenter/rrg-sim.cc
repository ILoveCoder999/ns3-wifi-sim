#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <set>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("RrgSim");

static const uint32_t N = 1331; // 对齐 3D Mesh 的 1331 台服务器
static const uint32_t D = 30;   // 对齐 3D Mesh 的单节点端口数 30

struct MeshProbe {
  double sentNs;
  double recvNs;
  uint16_t relays;
};
static std::map<uint64_t, MeshProbe> g_inflight;
static std::vector<MeshProbe> g_done;
static uint64_t g_sent = 0;

static double g_first_recv_time = -1.0;
static double g_last_recv_time = -1.0;
static uint64_t g_total_bits_transmitted = 0;

class RrgHost : public Application
{
public:
  static TypeId GetTypeId () {
    static TypeId t = TypeId ("RrgHost").SetParent<Application> ().AddConstructor<RrgHost> ();
    return t;
  }
  void Setup (uint32_t id, Ptr<Socket> rx, std::map<uint32_t, Address> addr) {
    m_id = id; m_rx = rx; m_addr = std::move (addr);
  }
  void Send (uint32_t dst, uint32_t bytes, uint64_t id) {
    Ptr<Packet> pkt = Create<Packet> (bytes);
    uint8_t b[12]; std::memcpy (b, &id, 8); std::memcpy (b + 8, &dst, 4);
    pkt->AddAtEnd (Create<Packet> (b, 12));

    MeshProbe pr;
    pr.sentNs = Simulator::Now ().GetNanoSeconds ();
    pr.recvNs = 0;
    pr.relays = 0;
    g_inflight[id] = pr;
    g_sent++;

    std::map<uint32_t, Address>::iterator it = m_addr.find (dst);
    if (it != m_addr.end ()) {
      if (!m_tx) m_tx = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
      m_tx->SendTo (pkt, 0, it->second);
    }
  }
private:
  void StartApplication () override { m_rx->SetRecvCallback (MakeCallback (&RrgHost::OnRecv, this)); }
  void StopApplication () override {}
  void OnRecv (Ptr<Socket> s) {
    Ptr<Packet> pkt;
    while ((pkt = s->Recv ())) {
      uint64_t id = 0; uint32_t fdst = 0;
      if (pkt->GetSize () >= 12) {
        uint8_t b[12]; Ptr<Packet> t = pkt->CreateFragment (pkt->GetSize () - 12, 12);
        t->CopyData (b, 12); std::memcpy (&id, b, 8); std::memcpy (&fdst, b + 8, 4);
      }
      double nowNs = Simulator::Now ().GetNanoSeconds ();
      if (g_first_recv_time < 0) g_first_recv_time = nowNs;
      g_last_recv_time = nowNs;

      std::map<uint64_t, MeshProbe>::iterator it = g_inflight.find (id);
      if (it != g_inflight.end ()) {
        it->second.recvNs = nowNs;
        g_done.push_back (it->second);
        g_inflight.erase (it);
      }
    }
  }
  uint32_t m_id {0}; Ptr<Socket> m_rx; Ptr<Socket> m_tx; std::map<uint32_t, Address> m_addr;
};

int main (int argc, char *argv[])
{
  std::string scenario = "uniform";
  uint32_t uniformFlows = 200000;
  std::string linkRate = "100Gbps";
  std::string linkDelay = "200ns";

  CommandLine cmd;
  cmd.AddValue ("scenario", "uniform|clique|hubs", scenario);
  cmd.AddValue ("uniformFlows", "number of random flows", uniformFlows);
  cmd.Parse (argc, argv);

  NodeContainer hosts; hosts.Create (N);
  InternetStackHelper inet; inet.Install (hosts);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (linkRate));
  p2p.SetChannelAttribute ("Delay", StringValue (linkDelay));
  p2p.SetQueue ("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue (QueueSize ("8p")));

  TrafficControlHelper tch; tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue ("8p"));

  Ipv4AddressHelper ip;
  std::map<uint32_t, Address> addr; std::map<uint32_t, Ipv4Address> firstIp;
  const uint16_t PORT = 9000; uint32_t subnet = 0;

  auto addEdge = [&] (uint32_t u, uint32_t v) {
    NetDeviceContainer dev = p2p.Install (hosts.Get (u), hosts.Get (v));
    tch.Install (dev);
    dev.Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (+[](Ptr<const Packet> p) { g_total_bits_transmitted += p->GetSize() * 8; }));
    dev.Get (1)->TraceConnectWithoutContext ("MacTx", MakeCallback (+[](Ptr<const Packet> p) { g_total_bits_transmitted += p->GetSize() * 8; }));

    uint32_t base = subnet * 4;
    std::ostringstream b;
    b << "10." << ((base >> 16) & 0xff) << "." << ((base >> 8) & 0xff) << "." << (base & 0xff);
    ip.SetBase (b.str ().c_str (), "255.255.255.252");
    Ipv4InterfaceContainer ic = ip.Assign (dev);
    if (!firstIp.count (u)) firstIp[u] = ic.GetAddress (0);
    if (!firstIp.count (v)) firstIp[v] = ic.GetAddress (1);
    ++subnet;
  };

  // FIX: 用标准 configuration model 生成 RRG，替换原版可能永久死锁的贪心算法。
  // 算法：生成 N*D 个 stub，Fisher-Yates 打乱，两两配对；若出现自环或重边则重试。
  // 对于 N=1331, D=30（N*D=39930 为偶数），期望重试次数极少。
  Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
  {
    std::vector<std::pair<uint32_t,uint32_t>> edges;
    int attempts = 0;
    while (true)
      {
        ++attempts;
        // 生成 stub 列表：每个节点 i 出现 D 次
        std::vector<uint32_t> stubs;
        stubs.reserve (N * D);
        for (uint32_t i = 0; i < N; ++i)
          for (uint32_t j = 0; j < D; ++j)
            stubs.push_back (i);

        // Fisher-Yates 随机打乱
        for (int64_t i = (int64_t)stubs.size () - 1; i > 0; --i)
          {
            uint32_t j = rng->GetInteger (0, (uint32_t)i);
            std::swap (stubs[i], stubs[j]);
          }

        // 两两配对，检查自环和重边
        bool valid = true;
        std::set<std::pair<uint32_t,uint32_t>> seen;
        edges.clear ();
        for (size_t i = 0; i + 1 < stubs.size (); i += 2)
          {
            uint32_t u = stubs[i], v = stubs[i + 1];
            if (u == v) { valid = false; break; }
            auto key = std::make_pair (std::min (u, v), std::max (u, v));
            if (seen.count (key)) { valid = false; break; }
            seen.insert (key);
            edges.push_back ({u, v});
          }
        if (valid) break;
      }
    std::cout << "RRG generated after " << attempts << " attempt(s)\n";
    for (auto &e : edges) addEdge (e.first, e.second);
  }

  std::cout << "Flat Random Graph (RRG) built. Total bidirectional links: " << subnet << "\n";

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  for (uint32_t i = 0; i < N; ++i) addr[i] = InetSocketAddress (firstIp[i], PORT);

  std::vector<Ptr<RrgHost>> apps (N);
  for (uint32_t i = 0; i < N; ++i) {
    Ptr<Socket> rx = Socket::CreateSocket (hosts.Get (i), TypeId::LookupByName ("ns3::UdpSocketFactory"));
    rx->Bind (InetSocketAddress (Ipv4Address::GetAny (), PORT));
    Ptr<RrgHost> a = CreateObject<RrgHost> ();
    a->Setup (i, rx, addr); hosts.Get (i)->AddApplication (a);
    a->SetStartTime (Seconds (0.0)); a->SetStopTime (Seconds (30.0));
    apps[i] = a;
  }

  uint64_t packet_id = 1;
  if (scenario == "clique") {
    std::vector<uint32_t> group;
    for(uint32_t i=0; i<N/5; ++i) group.push_back(i);
    double base = 1.0;
    for (uint32_t s : group) {
      for (uint32_t d : group) {
        if (s != d) {
          Simulator::Schedule (Seconds (base), &RrgHost::Send, apps[s], d, 1024, packet_id++);
          base += 1e-6;
        }
      }
    }
  } else if (scenario == "hubs") {
    std::vector<uint32_t> hubs = {10, 20, 30, 40};
    double base = 1.0;
    for (uint32_t s = 0; s < N; ++s) {
      for (uint32_t h : hubs) {
        if (s != h) {
          Simulator::Schedule (Seconds (base), &RrgHost::Send, apps[s], h, 1024, packet_id++);
          base += 1e-6;
        }
      }
    }
  } else {
    double when = 1.0;
    for (uint32_t k = 0; k < uniformFlows; ++k) {
      uint32_t s = rng->GetInteger (0, N - 1); uint32_t d = rng->GetInteger (0, N - 1);
      if (s == d) continue;
      Simulator::Schedule (Seconds (when), &RrgHost::Send, apps[s], d, 1024, packet_id++);
      when += 5e-7;
    }
  }

  double simDuration = 1.5;
  Simulator::Stop (Seconds (simDuration));
  Simulator::Run (); Simulator::Destroy ();

  uint64_t delivered = g_done.size ();
  uint64_t dropped = (g_sent >= delivered) ? (g_sent - delivered) : 0;
  double rxDurationSec = (g_last_recv_time > g_first_recv_time) ? (g_last_recv_time - g_first_recv_time) / 1e9 : 0;
  double throughputGbps = rxDurationSec > 0 ? (delivered * 1024 * 8.0) / (rxDurationSec * 1e9) : 0;

  double P_static_node = 2000.0; double P_full_load = 8000.0; double E_dynamic_bit = 10e-12;
  double staticEnergy = N * P_static_node * simDuration;
  double computeEnergy = rxDurationSec > 0 ? N * (P_full_load - P_static_node) * rxDurationSec : 0;
  double dynamicNetworkEnergy = g_total_bits_transmitted * E_dynamic_bit;
  double totalEnergy = staticEnergy + computeEnergy + dynamicNetworkEnergy;

  std::cout << "\n=== Flat Random Graph (RRG) Topology Result ===\n"
            << "Throughput : " << throughputGbps << " Gbps\n"
            << "Duration   : " << rxDurationSec << " s\n"
            << "Total Energy: " << totalEnergy << " J (Static: " << staticEnergy << " J, Compute: " << computeEnergy << " J)\n"
            << "Avg Power  : " << totalEnergy / simDuration << " W\n"
            << "Dropped    : " << dropped << " (" << (g_sent ? (100.0 * dropped / g_sent) : 0) << "%)\n";
  return 0;
}

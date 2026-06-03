#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("FatTreeSim");

static const uint32_t K = 18; // 选用 Radix=18 的交换机建立标准 3 层 Clos 架构
static const uint32_t total_pods = K;
static const uint32_t cores_per_group = (K / 2) * (K / 2);
static const uint32_t servers_per_tor = K / 2;
static const uint32_t tors_per_pod = K / 2;
static const uint32_t aggrs_per_pod = K / 2;
static const uint32_t N = 1331; // 仅向树底塞入 1331 台端服务器，其余不连端口留空

struct MeshProbe { double sentNs; double recvNs; uint16_t relays; };
static std::map<uint64_t, MeshProbe> g_inflight;
static std::vector<MeshProbe> g_done;
static uint64_t g_sent = 0;

static double g_first_recv_time = -1.0;
static double g_last_recv_time = -1.0;
static uint64_t g_total_bits_transmitted = 0;

class FatTreeHost : public Application
{
public:
  static TypeId GetTypeId () { static TypeId t = TypeId ("FatTreeHost").SetParent<Application>().AddConstructor<FatTreeHost>(); return t; }
  void Setup (uint32_t id, Ptr<Socket> rx, std::map<uint32_t, Address> addr) { m_id = id; m_rx = rx; m_addr = std::move (addr); }
  void Send (uint32_t dst, uint32_t bytes, uint64_t id) {
    Ptr<Packet> pkt = Create<Packet> (bytes);
    uint8_t b[12]; std::memcpy (b, &id, 8); std::memcpy (b + 8, &dst, 4); pkt->AddAtEnd (Create<Packet> (b, 12));
    MeshProbe pr; pr.sentNs = Simulator::Now ().GetNanoSeconds (); pr.recvNs = 0;
    g_inflight[id] = pr; g_sent++;
    std::map<uint32_t, Address>::iterator it = m_addr.find (dst);
    if (it != m_addr.end ()) {
      if (!m_tx) m_tx = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
      m_tx->SendTo (pkt, 0, it->second);
    }
  }
private:
  void StartApplication () override { m_rx->SetRecvCallback (MakeCallback (&FatTreeHost::OnRecv, this)); }
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
      if (it != g_inflight.end ()) { it->second.recvNs = nowNs; g_done.push_back (it->second); g_inflight.erase (it); }
    }
  }
  uint32_t m_id {0}; Ptr<Socket> m_rx; Ptr<Socket> m_tx; std::map<uint32_t, Address> m_addr;
};

int main (int argc, char *argv[])
{
  std::string scenario = "uniform";
  uint32_t uniformFlows = 200000;
  CommandLine cmd; cmd.AddValue ("scenario", "uniform|clique|hubs", scenario); cmd.Parse (argc, argv);

  // 1. 创建所有网络资产节点
  NodeContainer servers; servers.Create (N);
  NodeContainer tors; tors.Create (total_pods * tors_per_pod);
  NodeContainer aggrs; aggrs.Create (total_pods * aggrs_per_pod);
  NodeContainer cores; cores.Create (cores_per_group);

  InternetStackHelper inet;
  inet.Install (servers); inet.Install (tors); inet.Install (aggrs); inet.Install (cores);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("200ns"));
  p2p.SetQueue ("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue (QueueSize ("8p")));
  TrafficControlHelper tch; tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue ("8p"));

  Ipv4AddressHelper ip; uint32_t subnet = 0;
  std::map<uint32_t, Address> addr; std::map<uint32_t, Ipv4Address> firstIp;

  auto connect_nodes = [&] (Ptr<Node> u, Ptr<Node> v) {
    NetDeviceContainer dev = p2p.Install (u, v); tch.Install (dev);
    dev.Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (+[](Ptr<const Packet> p) { g_total_bits_transmitted += p->GetSize() * 8; }));
    dev.Get (1)->TraceConnectWithoutContext ("MacTx", MakeCallback (+[](Ptr<const Packet> p) { g_total_bits_transmitted += p->GetSize() * 8; }));
    uint32_t base = subnet * 4;
    std::ostringstream b; b << "10." << ((base >> 16) & 0xff) << "." << ((base >> 8) & 0xff) << "." << (base & 0xff);
    ip.SetBase (b.str ().c_str (), "255.255.255.252");
    Ipv4InterfaceContainer ic = ip.Assign (dev);
    return ic;
  };

  // 2. 胖树物理互联编织
  // 边缘层：连接服务器与 ToR 交换机
  uint32_t s_idx = 0;
  for (uint32_t p = 0; p < total_pods; ++p) {
    for (uint32_t t = 0; t < tors_per_pod; ++t) {
      uint32_t tor_node_id = p * tors_per_pod + t;
      for (uint32_t s = 0; s < servers_per_tor; ++s) {
        if (s_idx < N) {
          Ipv4InterfaceContainer ic = connect_nodes (servers.Get (s_idx), tors.Get (tor_node_id));
          firstIp[s_idx] = ic.GetAddress (0);
          s_idx++;
        }
      }
    }
  }

  // 汇聚层：Pod 内部 ToR 与 Agg 纵向全互联
  for (uint32_t p = 0; p < total_pods; ++p) {
    for (uint32_t t = 0; t < tors_per_pod; ++t) {
      uint32_t tor_id = p * tors_per_pod + t;
      for (uint32_t a = 0; a < aggrs_per_pod; ++a) {
        uint32_t agg_id = p * aggrs_per_pod + a;
        connect_nodes (tors.Get (tor_id), aggrs.Get (agg_id));
      }
    }
  }

  // 核心层：Agg 交换机与 Spine Core 跨 Pod 全互联
  for (uint32_t p = 0; p < total_pods; ++p) {
    for (uint32_t a = 0; a < aggrs_per_pod; ++a) {
      uint32_t agg_id = p * aggrs_per_pod + a;
      for (uint32_t c = 0; c < K / 2; ++c) {
        uint32_t core_id = a * (K / 2) + c;
        connect_nodes (aggrs.Get (agg_id), cores.Get (core_id));
      }
    }
  }
  std::cout << "3-Tier Clos Fat-Tree Built. Populating Routing tables...\n";

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  for (uint32_t i = 0; i < N; ++i) addr[i] = InetSocketAddress (firstIp[i], 9000);

  std::vector<Ptr<FatTreeHost>> apps (N);
  for (uint32_t i = 0; i < N; ++i) {
    Ptr<Socket> rx = Socket::CreateSocket (servers.Get (i), TypeId::LookupByName ("ns3::UdpSocketFactory"));
    rx->Bind (InetSocketAddress (Ipv4Address::GetAny (), 9000));
    Ptr<FatTreeHost> a = CreateObject<FatTreeHost> (); a->Setup (i, rx, addr);
    servers.Get (i)->AddApplication (a); a->SetStartTime (Seconds (0.0)); a->SetStopTime (Seconds (30.0));
    apps[i] = a;
  }

  Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
  uint64_t packet_id = 1;
  if (scenario == "clique") {
    std::vector<uint32_t> group; for(uint32_t i=0; i<N/5; ++i) group.push_back(i);
    double base = 1.0;
    for (uint32_t s : group) {
      for (uint32_t d : group) {
        if (s != d) { Simulator::Schedule (Seconds (base), &FatTreeHost::Send, apps[s], d, 1024, packet_id++); base += 1e-6; }
      }
    }
  } else if (scenario == "hubs") {
    std::vector<uint32_t> hubs = {10, 20, 30, 40}; double base = 1.0;
    for (uint32_t s = 0; s < N; ++s) {
      for (uint32_t h : hubs) {
        if (s != h) { Simulator::Schedule (Seconds (base), &FatTreeHost::Send, apps[s], h, 1024, packet_id++); base += 1e-6; }
      }
    }
  } else {
    double when = 1.0;
    for (uint32_t k = 0; k < uniformFlows; ++k) {
      uint32_t s = rng->GetInteger (0, N - 1); uint32_t d = rng->GetInteger (0, N - 1);
      if (s == d) continue;
      Simulator::Schedule (Seconds (when), &FatTreeHost::Send, apps[s], d, 1024, packet_id++);
      when += 5e-7;
    }
  }

  double simDuration = 1.5;
  Simulator::Stop (Seconds (simDuration));
  Simulator::Run (); Simulator::Destroy ();

  uint64_t delivered = g_done.size (); uint64_t dropped = (g_sent >= delivered) ? (g_sent - delivered) : 0;
  double rxDurationSec = (g_last_recv_time > g_first_recv_time) ? (g_last_recv_time - g_first_recv_time) / 1e9 : 0;
  double throughputGbps = rxDurationSec > 0 ? (delivered * 1024 * 8.0) / (rxDurationSec * 1e9) : 0;

  // 胖树的特殊学术指标：计算由于多出来的独立交换机硬件引入的大量静态功耗开销！
  uint32_t total_switches = tors.GetN() + aggrs.GetN() + cores.GetN(); // 162 + 162 + 81 = 405 台核心物理交换机 [cite: 622, 653]
  double P_switch_static = 150.0; // 工业级高速 100G 交换机基础静态待机功率：150 W
  
  double P_server_static = 2000.0; double P_server_full = 8000.0; double E_dynamic_bit = 10e-12;
  double staticEnergy = (N * P_server_static + total_switches * P_switch_static) * simDuration;
  double computeEnergy = rxDurationSec > 0 ? N * (P_server_full - P_server_static) * rxDurationSec : 0;
  double dynamicNetworkEnergy = g_total_bits_transmitted * E_dynamic_bit;
  double totalEnergy = staticEnergy + computeEnergy + dynamicNetworkEnergy;

  std::cout << "\n=== 3-Tier Fat-Tree Topology Result ===\n"
            << "Throughput : " << throughputGbps << " Gbps\n"
            << "Duration   : " << rxDurationSec << " s\n"
            << "Total Energy: " << totalEnergy << " J (含 " << total_switches << "台交换机常驻开销)\n"
            << "Avg Power  : " << totalEnergy / simDuration << " W\n"
            << "Dropped    : " << dropped << " (" << (g_sent ? (100.0 * dropped / g_sent) : 0) << "%)\n";
  return 0;
}
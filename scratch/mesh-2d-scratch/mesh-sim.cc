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

#include "mesh-route-header.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("MeshSim");

static const uint32_t A = 17;
static uint32_t NID (uint32_t r, uint32_t c) { return r * A + c; }
static void RC (uint32_t id, uint32_t &r, uint32_t &c) { r = id / A; c = id % A; }

static std::vector<uint16_t> Dor (uint32_t s, uint32_t d)
{
  uint32_t rs, cs, rd, cd;
  RC (s, rs, cs);
  RC (d, rd, cd);
  if (s == d) return { (uint16_t) s };
  if (rs == rd || cs == cd) return { (uint16_t) s, (uint16_t) d };
  return { (uint16_t) s, (uint16_t) NID (rs, cd), (uint16_t) d };
}

struct MeshProbe
{
  double sentNs;
  double recvNs;
  uint16_t relays;
};
static std::map<uint64_t, MeshProbe> g_inflight;
static std::vector<MeshProbe> g_done;
static uint64_t g_sent = 0;

// 全局统计变量
static double g_first_recv_time = -1.0;
static double g_last_recv_time = -1.0;
static uint64_t g_total_bits_transmitted = 0; 

class MeshHost : public Application
{
public:
  static TypeId GetTypeId ()
  {
    static TypeId t = TypeId ("MeshHost")
      .SetParent<Application> ()
      .AddConstructor<MeshHost> ();
    return t;
  }

  void Setup (uint32_t id, Ptr<Socket> rx, std::map<uint32_t, Address> addr)
  {
    m_id = id;
    m_rx = rx;
    m_addr = std::move (addr);
  }

  void Send (uint32_t dst, uint32_t bytes, uint64_t id)
  {
    std::vector<uint16_t> path = Dor (m_id, dst);
    MeshRouteHeader h;
    h.SetPath (path);
    Ptr<Packet> pkt = Create<Packet> (bytes);
    pkt->AddHeader (h);
    AppendId (pkt, id, dst);
    uint16_t relays = (path.size () >= 2) ? (uint16_t) (path.size () - 2) : 0;
    MeshProbe pr;
    pr.sentNs = Simulator::Now ().GetNanoSeconds ();
    pr.recvNs = 0;
    pr.relays = relays;
    g_inflight[id] = pr;
    g_sent++;
    SendTo (path[1], pkt);
  }

private:
  void StartApplication () override
  {
    m_rx->SetRecvCallback (MakeCallback (&MeshHost::OnRecv, this));
  }
  void StopApplication () override {}

  static void AppendId (Ptr<Packet> p, uint64_t id, uint32_t dst)
  {
    uint8_t b[12];
    std::memcpy (b, &id, 8);
    std::memcpy (b + 8, &dst, 4);
    p->AddAtEnd (Create<Packet> (b, 12));
  }

  void OnRecv (Ptr<Socket> s)
  {
    Ptr<Packet> pkt;
    while ((pkt = s->Recv ()))
      {
        uint64_t id = 0;
        uint32_t fdst = 0;
        if (pkt->GetSize () >= 12)
          {
            uint8_t b[12];
            Ptr<Packet> t = pkt->CreateFragment (pkt->GetSize () - 12, 12);
            t->CopyData (b, 12);
            std::memcpy (&id, b, 8);
            std::memcpy (&fdst, b + 8, 4);
            pkt->RemoveAtEnd (12);
          }
        MeshRouteHeader h;
        pkt->RemoveHeader (h);
        const std::vector<uint16_t> &path = h.GetPath ();

        // 终点服务器判断：只有当本服务器是路径的最后一跳时，才终止接收并计入多跳时延
        if (m_id == path.back())
          {
            double nowNs = Simulator::Now ().GetNanoSeconds ();
            if (g_first_recv_time < 0) g_first_recv_time = nowNs;
            g_last_recv_time = nowNs;

            std::map<uint64_t, MeshProbe>::iterator it = g_inflight.find (id);
            if (it != g_inflight.end ())
              {
                it->second.recvNs = nowNs;
                g_done.push_back (it->second);
                g_inflight.erase (it);
              }
            continue;
          }

        // 中间服务器路由转发：基于当前 ID 在 path 中的位置，寻找下一跳服务器
        uint16_t next_hop = 0;
        bool found = false;
        for (size_t i = 0; i < path.size() - 1; ++i)
          {
            if (path[i] == m_id)
              {
                next_hop = path[i + 1];
                found = true;
                h.SetCursor(i + 1); 
                break;
              }
          }

        if (found)
          {
            pkt->AddHeader (h);
            AppendId (pkt, id, fdst);
            SendTo (next_hop, pkt);
          }
      }
  }

  void SendTo (uint16_t nid, Ptr<Packet> pkt)
  {
    std::map<uint32_t, Address>::iterator it = m_addr.find (nid);
    if (it == m_addr.end ()) return;
    if (!m_tx)
      m_tx = Socket::CreateSocket (GetNode (),
               TypeId::LookupByName ("ns3::UdpSocketFactory"));
    
    // 累加网络在物理链路上实际发送的比特数
    g_total_bits_transmitted += pkt->GetSize () * 8;
    m_tx->SendTo (pkt, 0, it->second);
  }

  uint32_t m_id {0};
  Ptr<Socket> m_rx;
  Ptr<Socket> m_tx;
  std::map<uint32_t, Address> m_addr;
};

int main (int argc, char *argv[])
{
  std::string scenario = "incast";
  bool schedule = true;
  uint32_t fanin = 64;
  uint32_t pktBytes = 1024;
  uint32_t queuePkts = 64;
  std::string linkRate = "100Gbps";
  std::string linkDelay = "200ns";

  CommandLine cmd;
  cmd.AddValue ("scenario", "uniform|incast|hotspot", scenario);
  cmd.AddValue ("schedule", "1=paced (lossless), 0=burst (drops)", schedule);
  cmd.AddValue ("fanin", "incast fan-in", fanin);
  cmd.AddValue ("pktBytes", "payload bytes", pktBytes);
  cmd.AddValue ("queuePkts", "finite link queue depth (packets)", queuePkts);
  cmd.AddValue ("linkRate", "link rate", linkRate);
  cmd.AddValue ("linkDelay", "link delay", linkDelay);
  cmd.Parse (argc, argv);

  uint32_t N = A * A;
  NodeContainer hosts;
  hosts.Create (N);
  InternetStackHelper inet;
  inet.Install (hosts);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (linkRate));
  p2p.SetChannelAttribute ("Delay", StringValue (linkDelay));
  std::ostringstream qs;
  qs << queuePkts << "p";
  p2p.SetQueue ("ns3::DropTailQueue<Packet>",
                "MaxSize", QueueSizeValue (QueueSize (qs.str ())));

  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::FifoQueueDisc",
                        "MaxSize", StringValue ("8p"));

  Ipv4AddressHelper ip;
  std::map<uint32_t, Address> addr;
  std::map<uint32_t, Ipv4Address> firstIp;
  const uint16_t PORT = 9000;
  uint32_t subnet = 0;

  std::function<void(uint32_t, uint32_t)> addEdge =
    [&] (uint32_t u, uint32_t v)
    {
      if (v <= u) return;
      NetDeviceContainer dev = p2p.Install (hosts.Get (u), hosts.Get (v));
      tch.Install (dev);
      std::ostringstream b;
      b << "10." << ((subnet >> 8) & 0xff) << "." << (subnet & 0xff) << ".0";
      ip.SetBase (b.str ().c_str (), "255.255.255.252");
      Ipv4InterfaceContainer ic = ip.Assign (dev);
      if (!firstIp.count (u)) firstIp[u] = ic.GetAddress (0);
      if (!firstIp.count (v)) firstIp[v] = ic.GetAddress (1);
      ++subnet;
    };

  for (uint32_t r = 0; r < A; ++r)
    for (uint32_t c = 0; c < A; ++c)
      {
        uint32_t u = NID (r, c);
        for (uint32_t c2 = 0; c2 < A; ++c2)
          if (c2 != c) addEdge (u, NID (r, c2));
        for (uint32_t r2 = 0; r2 < A; ++r2)
          if (r2 != r) addEdge (u, NID (r2, c));
      }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  for (uint32_t i = 0; i < N; ++i)
    addr[i] = InetSocketAddress (firstIp[i], PORT);

  std::vector<Ptr<MeshHost>> apps (N);
  for (uint32_t i = 0; i < N; ++i)
    {
      Ptr<Socket> rx = Socket::CreateSocket (hosts.Get (i),
                          TypeId::LookupByName ("ns3::UdpSocketFactory"));
      rx->Bind (InetSocketAddress (Ipv4Address::GetAny (), PORT));
      Ptr<MeshHost> a = CreateObject<MeshHost> ();
      a->Setup (i, rx, addr);
      hosts.Get (i)->AddApplication (a);
      a->SetStartTime (Seconds (0.0));
      a->SetStopTime (Seconds (20.0));
      apps[i] = a;
    }

  Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
  uint64_t id = 1;
  double serSec = (pktBytes * 8.0) / 100e9;

  if (scenario == "incast")
    {
      uint32_t dst = NID (A / 2, A / 2);
      double base = 1.0;
      for (uint32_t k = 0; k < fanin; ++k)
        {
          uint32_t s = (dst + 1 + k) % N;
          for (uint32_t m = 0; m < 64; ++m)
            {
              double when;
              if (schedule)
                when = base + (k * 64 + m) * serSec;
              else
                when = base + (m * 1e-9);
              Simulator::Schedule (Seconds (when), &MeshHost::Send, apps[s],
                                   dst, pktBytes, id++);
            }
        }
    }
  else if (scenario == "relaycongest")
    {
      uint32_t rd = 8, cd = 8, RX = 3;
      uint32_t D = NID (rd, cd);
      double base = 1.0;
      for (uint32_t c = 0; c < A; ++c)
        {
          if (c == cd) continue;
          uint32_t s = NID (RX, c);
          for (uint32_t m = 0; m < 256; ++m)
            {
              double when;
              if (schedule)
                when = base + (c * 256 + m) * serSec;
              else
                when = base + (m * 1e-9);
              Simulator::Schedule (Seconds (when), &MeshHost::Send, apps[s],
                                   D, pktBytes, id++);
            }
        }
    }
  else if (scenario == "hotspot")
    {
      std::vector<uint32_t> hot = { NID (0,0), NID (0,1), NID (1,0), NID (1,1) };
      double base = 1.0; uint32_t cnt = 0;
      for (uint32_t s = 0; s < N; ++s)
        for (uint32_t j = 0; j < hot.size (); ++j)
          if (s != hot[j])
            {
              double when = schedule ? base + (cnt++) * serSec : base + 1e-9;
              Simulator::Schedule (Seconds (when), &MeshHost::Send, apps[s],
                                   hot[j], pktBytes, id++);
            }
    }
  else
    {
      // uniform 场景：生成 20 万个随机包
      double when = 1.0;
      for (uint32_t k = 0; k < 200000; ++k)
        {
          uint32_t s = rng->GetInteger (0, N - 1);
          uint32_t d = rng->GetInteger (0, N - 1);
          if (s == d) continue;
          Simulator::Schedule (Seconds (when), &MeshHost::Send, apps[s],
                               d, pktBytes, id++);
          when += 1e-6; // 步长1微秒，20W包总持续打流时长为0.2秒
        }
    }

  // 模拟设定时长为 1.5 秒
  double simDuration = 1.5;
  Simulator::Stop (Seconds (simDuration));
  Simulator::Run ();
  Simulator::Destroy ();

  uint64_t delivered = g_done.size ();
  uint64_t dropped = (g_sent >= delivered) ? (g_sent - delivered) : 0;

  double rxDurationSec = 0.0;
  double throughputGbps = 0.0;
  if (g_last_recv_time > g_first_recv_time && g_first_recv_time > 0)
    {
      rxDurationSec = (g_last_recv_time - g_first_recv_time) / 1e9;
      throughputGbps = (delivered * pktBytes * 8.0) / (rxDurationSec * 1e9);
    }

  // =================================================================
  // 【新模型实现】：精准的“静态2000W / 满载8000W”分段能耗模型
  // =================================================================
  double P_static_node = 2000.0;   // 基础静态功率：2000 W
  double P_full_load   = 8000.0;   // 满载最大功率：8000 W
  double P_compute     = P_full_load - P_static_node; // 满载额外计算功率：6000 W
  double E_dynamic_bit = 10e-12;   // 高速模块动态线传能耗：10 pJ/bit

  // 基础静态能耗（1.5s 周期内一直产生）
  double staticEnergy = N * P_static_node * simDuration;
  
  // 满载额外计算能耗（仅在真正通信忙碌的 rxDurationSec 约 0.2s 内产生）
  double computeEnergy = 0.0;
  if (rxDurationSec > 0)
    {
      computeEnergy = N * P_compute * rxDurationSec;
    }
    
  // 网卡链路传输产生的网络动态能耗
  double dynamicNetworkEnergy = g_total_bits_transmitted * E_dynamic_bit;
  
  // 累加总能量及计算在 1.5s 周期内的平均总功率
  double totalEnergy = staticEnergy + computeEnergy + dynamicNetworkEnergy; 
  double avgPowerW   = totalEnergy / simDuration;
  // =================================================================

  std::map<uint16_t, std::vector<double>> byRelay;
  for (uint32_t i = 0; i < g_done.size (); ++i)
    if (g_done[i].recvNs > 0)
      byRelay[g_done[i].relays].push_back (g_done[i].recvNs - g_done[i].sentNs);

  std::cout << "\n=== scenario=" << scenario << " schedule=" << schedule
            << " fanin=" << fanin << " queue=" << queuePkts << "p ===\n";
  std::cout << "sent=" << g_sent << " delivered=" << delivered
            << " dropped=" << dropped
            << " (" << (g_sent ? (100.0 * dropped / g_sent) : 0) << "%)\n";
  
  std::cout << "-------------------------------------------\n";
  std::cout << "吞吐量 (Throughput)     : " << throughputGbps << " Gbps\n";
  std::cout << "接收有效时长 (Duration)  : " << rxDurationSec << " s\n";
  std::cout << "全网总能耗 (Total Energy): " << totalEnergy << " J\n";
  std::cout << "  ├─ 服务器基础静态能耗(Static) : " << staticEnergy << " J (2000W 基础)\n";
  std::cout << "  ├─ GPU满载额外计算能耗(Compute): " << computeEnergy << " J (突发 8000W 增量)\n";
  std::cout << "  └─ 网卡/光模块动态传输(Network): " << dynamicNetworkEnergy << " J\n";
  std::cout << "全网平均总功耗 (Avg Power): " << avgPowerW << " W\n";
  std::cout << "-------------------------------------------\n";

  std::cout << "relay  count   mean_us   p99_us   max_us\n";
  for (std::map<uint16_t, std::vector<double>>::iterator kv = byRelay.begin ();
       kv != byRelay.end (); ++kv)
    {
      std::vector<double> v = kv->second;
      std::sort (v.begin (), v.end ());
      double sum = 0;
      for (uint32_t i = 0; i < v.size (); ++i) sum += v[i];
      double mean = sum / v.size ();
      double p99 = v[std::min ((size_t) (v.size () - 1), v.size () * 99 / 100)];
      double max_val = v.back(); 
      printf ("%3u   %6zu  %8.3f  %8.3f  %8.3f\n", kv->first, v.size (),
              mean / 1000, p99 / 1000, max_val / 1000);
    }

  std::ofstream csv ("mesh_result.csv");
  csv << "scenario,schedule,fanin,sent,delivered,dropped,throughput_Gbps,total_energy_J,avg_power_W\n";
  csv << scenario << "," << schedule << "," << fanin << ","
      << g_sent << "," << delivered << "," << dropped << ","
      << throughputGbps << "," << totalEnergy << "," << avgPowerW << "\n";
  std::cout << "wrote mesh_result.csv\n";
  return 0;
}
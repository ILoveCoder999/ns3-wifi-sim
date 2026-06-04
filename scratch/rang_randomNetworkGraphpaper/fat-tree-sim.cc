/*
 * fat-tree-sim.cc — RoCEv2 完整实现
 * ─────────────────────────────────────────────────────────────────────────
 * 拓扑：三层无收敛胖树（3-Tier Clos Fat-Tree）
 *   Server → ToR (Leaf) → Aggregation → Core (Spine)
 *   K=18：18 pods × 9 ToR × 9 servers = 1458 服务器（非阻塞）
 *
 * RoCEv2 三层机制（与 leaf-spine-sim.cc 完全对称）
 * ────────────────────────────────────────────────
 *  1. 无损传输：物理队列 256p + RED(20/60p) 先标记后丢
 *  2. ECN：发端 ECT(0)，交换机标 CE，收端 SocketIpTosTag 检测
 *  3. DCQCN：CNP 反压 + alpha 乘性降速 + RT 定时器加性恢复 + per-dst 限速器
 *
 * 流量场景（--scenario=）
 *   uniform / incast / allreduce / bisection
 */

#include <algorithm>
#include <cstring>
#include <deque>
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
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/socket.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("FatTreeRocev2");

static const uint16_t DATA_PORT = 9000;
static const uint16_t CNP_PORT  = 9001;

// ─── 拓扑常量 ──────────────────────────────────────────────────────────────
static const uint32_t K               = 18;
static const uint32_t total_pods      = K;
static const uint32_t nCore           = (K/2) * (K/2);  // 81
static const uint32_t servers_per_tor = K/2;             // 9
static const uint32_t tors_per_pod    = K/2;             // 9
static const uint32_t aggrs_per_pod   = K/2;             // 9
static const uint32_t N               = total_pods * tors_per_pod * servers_per_tor; // 1458

// ─── 全局统计 ──────────────────────────────────────────────────────────────
struct Probe { double sentNs; double recvNs; };
static std::map<uint64_t, Probe> g_inflight;
static std::vector<Probe>        g_done;
static uint64_t                  g_sent      = 0;
static double                    g_first_recv = -1.0;
static double                    g_last_recv  = -1.0;
static uint64_t                  g_total_bits = 0;
static uint64_t                  g_cnp_sent   = 0;
static uint64_t                  g_cnp_recv   = 0;
static uint64_t                  g_ce_pkts    = 0;

// ─── DCQCN 参数 ────────────────────────────────────────────────────────────
namespace Dcqcn {
  static constexpr double   G          = 1.0/16.0;
  static constexpr double   RAI        = 40e6;
  static constexpr uint64_t RT_US      = 55;
  static constexpr double   MIN_RATE   = 100e6;
  static constexpr uint64_t CNP_GAP_US = 50;
}

// ─── RoCEv2 Host（与 leaf-spine-sim.cc 完全相同的 DCQCN 实现）────────────
class RoceHost : public Application
{
public:
  static TypeId GetTypeId ()
  {
    static TypeId t = TypeId ("RoceHost")
      .SetParent<Application> ()
      .AddConstructor<RoceHost> ();
    return t;
  }

  void Setup (uint32_t id, Ptr<Socket> dataRx, Ptr<Socket> cnpRx,
              std::map<uint32_t, Address> addr, double lineRateBps)
  {
    m_id       = id;
    m_dataRx   = dataRx;
    m_cnpRx    = cnpRx;
    m_addr     = std::move (addr);
    m_lineRate = lineRateBps;
  }

  void EnqueueAndSend (uint32_t dst, uint32_t bytes, uint64_t id)
  {
    if (m_id == dst) return;
    m_txQueue[dst].push_back ({dst, bytes, id});
    if (!m_pacing[dst])
      { m_pacing[dst] = true; Simulator::ScheduleNow (&RoceHost::DoSendPkt, this, dst); }
  }

private:
  struct Pending { uint32_t dst; uint32_t bytes; uint64_t id; };

  struct DcqcnState
  {
    double  rate    = 0;
    double  alpha   = 1.0;
    bool    cnpFlag = false;
    EventId rtTimer;
  };

  void StartApplication () override
  {
    m_dataRx->SetIpRecvTos (true);
    m_dataRx->SetRecvCallback (MakeCallback (&RoceHost::OnDataRecv, this));
    m_cnpRx->SetRecvCallback  (MakeCallback (&RoceHost::OnCnpRecv,  this));
  }
  void StopApplication () override {}

  // ── 限速发包链 ──
  void DoSendPkt (uint32_t dst)
  {
    auto &q = m_txQueue[dst];
    if (q.empty ()) { m_pacing[dst] = false; return; }
    Pending p = q.front (); q.pop_front ();
    ActuallySendData (p.dst, p.bytes, p.id);
    double rate   = GetRate (dst);
    double gapSec = ((p.bytes + 12) * 8.0) / rate;
    Simulator::Schedule (Seconds (gapSec), &RoceHost::DoSendPkt, this, dst);
  }

  double GetRate (uint32_t dst)
  {
    auto it = m_dcqcn.find (dst);
    return (it == m_dcqcn.end () || it->second.rate <= 0) ? m_lineRate : it->second.rate;
  }

  // ── 实际发包：设 ECT(0) ──
  void ActuallySendData (uint32_t dst, uint32_t bytes, uint64_t id)
  {
    Ptr<Packet> pkt = Create<Packet> (bytes);
    uint8_t b[12];
    std::memcpy (b,     &id,  8);
    std::memcpy (b + 8, &dst, 4);
    pkt->AddAtEnd (Create<Packet> (b, 12));

    Probe pr; pr.sentNs = Simulator::Now ().GetNanoSeconds (); pr.recvNs = 0;
    g_inflight[id] = pr;
    g_sent++;

    auto it = m_addr.find (dst);
    if (it == m_addr.end ()) return;
    if (!m_dataTx)
      {
        m_dataTx = Socket::CreateSocket (GetNode (),
                     TypeId::LookupByName ("ns3::UdpSocketFactory"));
        m_dataTx->SetIpTos (0x02);   // ECT(0)：允许网络打 CE 标记
      }
    m_dataTx->SendTo (pkt, 0, it->second);
  }

  // ── 数据接收：检测 CE，限速发 CNP ──
  void OnDataRecv (Ptr<Socket> s)
  {
    Address from;
    Ptr<Packet> pkt;
    while ((pkt = s->RecvFrom (from)))
      {
        SocketIpTosTag tosTag;
        bool hasTos = pkt->RemovePacketTag (tosTag);
        bool isCE   = hasTos && ((tosTag.GetTos () & 0x03) == 0x03);

        if (pkt->GetSize () < 12) continue;
        uint8_t b[12];
        pkt->CreateFragment (pkt->GetSize () - 12, 12)->CopyData (b, 12);
        uint64_t id; std::memcpy (&id, b, 8);

        double nowNs = Simulator::Now ().GetNanoSeconds ();
        if (g_first_recv < 0) g_first_recv = nowNs;
        g_last_recv = nowNs;

        auto it = g_inflight.find (id);
        if (it != g_inflight.end ())
          { it->second.recvNs = nowNs; g_done.push_back (it->second); g_inflight.erase (it); }

        if (isCE)
          {
            ++g_ce_pkts;
            InetSocketAddress inet = InetSocketAddress::ConvertFrom (from);
            Ipv4Address srcIp = inet.GetIpv4 ();
            Time now = Simulator::Now ();
            auto &lastT = m_lastCnpTime[srcIp];
            if (lastT.IsZero () || now - lastT >= MicroSeconds (Dcqcn::CNP_GAP_US))
              { SendCnp (srcIp); lastT = now; }
          }
      }
  }

  void SendCnp (Ipv4Address targetIp)
  {
    if (!m_cnpTx)
      m_cnpTx = Socket::CreateSocket (GetNode (),
                  TypeId::LookupByName ("ns3::UdpSocketFactory"));
    uint8_t b[4]; std::memcpy (b, &m_id, 4);
    Ptr<Packet> cnp = Create<Packet> (b, 4);
    m_cnpTx->SendTo (cnp, 0, InetSocketAddress (targetIp, CNP_PORT));
    ++g_cnp_sent;
  }

  // ── CNP 接收：DCQCN 乘性降速 ──
  void OnCnpRecv (Ptr<Socket> s)
  {
    Ptr<Packet> pkt;
    while ((pkt = s->Recv ()))
      {
        if (pkt->GetSize () < 4) continue;
        uint8_t b[4]; pkt->CopyData (b, 4);
        uint32_t receiverId; std::memcpy (&receiverId, b, 4);
        ++g_cnp_recv;
        ApplyDcqcnDecrease (receiverId);
      }
  }

  void ApplyDcqcnDecrease (uint32_t dst)
  {
    auto &st = m_dcqcn[dst];
    if (st.rate <= 0) st.rate = m_lineRate;
    st.alpha  = (1.0 - Dcqcn::G) * st.alpha + Dcqcn::G;
    st.rate  *= (1.0 - st.alpha / 2.0);
    st.rate   = std::max (st.rate, Dcqcn::MIN_RATE);
    st.cnpFlag = true;
    st.rtTimer.Cancel ();
    st.rtTimer = Simulator::Schedule (
      MicroSeconds (Dcqcn::RT_US), &RoceHost::DcqcnRateTimer, this, dst);
  }

  void DcqcnRateTimer (uint32_t dst)
  {
    auto &st = m_dcqcn[dst];
    st.alpha = (1.0 - Dcqcn::G) * st.alpha;
    if (st.cnpFlag)
      st.cnpFlag = false;
    else
      st.rate = std::min (st.rate + Dcqcn::RAI, m_lineRate);
    if (st.rate < m_lineRate * 0.999)
      st.rtTimer = Simulator::Schedule (
        MicroSeconds (Dcqcn::RT_US), &RoceHost::DcqcnRateTimer, this, dst);
  }

  uint32_t m_id {0};
  double   m_lineRate {400e9};
  Ptr<Socket> m_dataRx, m_cnpRx, m_dataTx, m_cnpTx;
  std::map<uint32_t, Address>             m_addr;
  std::map<uint32_t, std::deque<Pending>> m_txQueue;
  std::map<uint32_t, bool>                m_pacing;
  std::map<uint32_t, DcqcnState>          m_dcqcn;
  std::map<Ipv4Address, Time>             m_lastCnpTime;
};

// ─── main ──────────────────────────────────────────────────────────────────
int main (int argc, char *argv[])
{
  std::string scenario     = "uniform";
  uint32_t    uniformFlows = 200000;
  uint32_t    incastFanin  = 64;
  uint32_t    pktBytes     = 1024;
  uint32_t    queuePkts    = 256;
  uint32_t    ecnMinTh     = 20;
  uint32_t    ecnMaxTh     = 60;
  std::string linkRate     = "400Gbps";
  std::string linkDelay    = "200ns";

  CommandLine cmd;
  cmd.AddValue ("scenario",     "uniform|incast|allreduce|bisection", scenario);
  cmd.AddValue ("uniformFlows", "number of random flows",    uniformFlows);
  cmd.AddValue ("incastFanin",  "incast fan-in",             incastFanin);
  cmd.AddValue ("pktBytes",     "payload bytes",             pktBytes);
  cmd.AddValue ("queuePkts",    "physical queue depth",      queuePkts);
  cmd.AddValue ("ecnMinTh",     "RED ECN MinTh (pkts)",      ecnMinTh);
  cmd.AddValue ("ecnMaxTh",     "RED ECN MaxTh (pkts)",      ecnMaxTh);
  cmd.AddValue ("linkRate",     "link rate",                 linkRate);
  cmd.AddValue ("linkDelay",    "link delay",                linkDelay);
  cmd.Parse (argc, argv);

  DataRate dr (linkRate);
  double lineRateBps = (double) dr.GetBitRate ();

  Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting",
                      BooleanValue (true));

  std::cout << "=== 3-Tier Fat-Tree + RoCEv2 (DCQCN), K=" << K << " ===\n"
            << "  " << total_pods << " pods | "
            << total_pods*tors_per_pod << " ToR | "
            << total_pods*aggrs_per_pod << " Agg | "
            << nCore << " Core | " << N << " servers\n"
            << "  Link: " << linkRate << " / " << linkDelay << "\n"
            << "  Lossless: DropTail " << queuePkts << "p  "
            << "ECN: RED " << ecnMinTh << "/" << ecnMaxTh << "p  "
            << "CC: DCQCN\n\n";

  // ── 节点 ──
  NodeContainer servers; servers.Create (N);
  NodeContainer tors;    tors.Create    (total_pods * tors_per_pod);
  NodeContainer aggrs;   aggrs.Create   (total_pods * aggrs_per_pod);
  NodeContainer cores;   cores.Create   (nCore);

  InternetStackHelper inet;
  inet.Install (servers); inet.Install (tors);
  inet.Install (aggrs);   inet.Install (cores);

  // ── 链路模板：深物理队列 + RED+ECN ──
  std::ostringstream qs; qs << queuePkts << "p";
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute  ("DataRate", StringValue (linkRate));
  p2p.SetChannelAttribute ("Delay",    StringValue (linkDelay));
  p2p.SetQueue ("ns3::DropTailQueue<Packet>",
                "MaxSize", QueueSizeValue (QueueSize (qs.str ())));

  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                        "MinTh",   DoubleValue (ecnMinTh),
                        "MaxTh",   DoubleValue (ecnMaxTh),
                        "UseEcn",  BooleanValue (true),
                        "MaxSize", QueueSizeValue (QueueSize (qs.str ())));

  Ipv4AddressHelper ip;
  std::map<uint32_t, Ipv4Address> serverIp;
  uint32_t subnet = 0;

  auto connect_nodes = [&] (Ptr<Node> u, Ptr<Node> v, Ipv4Address *ipU = nullptr)
  {
    NetDeviceContainer dev = p2p.Install (u, v);
    tch.Install (dev);
    dev.Get (0)->TraceConnectWithoutContext ("MacTx",
      MakeCallback (+[](Ptr<const Packet> p){ g_total_bits += p->GetSize ()*8; }));
    dev.Get (1)->TraceConnectWithoutContext ("MacTx",
      MakeCallback (+[](Ptr<const Packet> p){ g_total_bits += p->GetSize ()*8; }));
    uint32_t base = subnet * 4;
    std::ostringstream b;
    b << "10." << ((base>>16)&0xff) << "." << ((base>>8)&0xff) << "." << (base&0xff);
    ip.SetBase (b.str ().c_str (), "255.255.255.252");
    Ipv4InterfaceContainer ic = ip.Assign (dev);
    if (ipU) *ipU = ic.GetAddress (0);
    ++subnet;
    return ic;
  };

  // ── 三层互联 ──
  uint32_t s_idx = 0;
  for (uint32_t p = 0; p < total_pods; ++p)
    for (uint32_t t = 0; t < tors_per_pod; ++t)
      {
        uint32_t tor_id = p * tors_per_pod + t;
        for (uint32_t s = 0; s < servers_per_tor && s_idx < N; ++s, ++s_idx)
          { Ipv4Address sip; connect_nodes (servers.Get (s_idx), tors.Get (tor_id), &sip); serverIp[s_idx] = sip; }
      }
  for (uint32_t p = 0; p < total_pods; ++p)
    for (uint32_t t = 0; t < tors_per_pod; ++t)
      for (uint32_t a = 0; a < aggrs_per_pod; ++a)
        connect_nodes (tors.Get (p*tors_per_pod+t), aggrs.Get (p*aggrs_per_pod+a));
  for (uint32_t p = 0; p < total_pods; ++p)
    for (uint32_t a = 0; a < aggrs_per_pod; ++a)
      for (uint32_t c = 0; c < K/2; ++c)
        connect_nodes (aggrs.Get (p*aggrs_per_pod+a), cores.Get (a*(K/2)+c));

  std::cout << "  Links built: " << subnet << " total\n";
  std::cout << "  Populating routing tables...\n";
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  std::map<uint32_t, Address> addr;
  for (uint32_t i = 0; i < N; ++i)
    addr[i] = InetSocketAddress (serverIp[i], DATA_PORT);

  // ── 安装应用（每节点：数据 RX + CNP RX）──
  std::vector<Ptr<RoceHost>> apps (N);
  for (uint32_t i = 0; i < N; ++i)
    {
      Ptr<Socket> dataRx = Socket::CreateSocket (servers.Get (i),
                             TypeId::LookupByName ("ns3::UdpSocketFactory"));
      dataRx->Bind (InetSocketAddress (Ipv4Address::GetAny (), DATA_PORT));

      Ptr<Socket> cnpRx = Socket::CreateSocket (servers.Get (i),
                            TypeId::LookupByName ("ns3::UdpSocketFactory"));
      cnpRx->Bind (InetSocketAddress (Ipv4Address::GetAny (), CNP_PORT));

      Ptr<RoceHost> a = CreateObject<RoceHost> ();
      a->Setup (i, dataRx, cnpRx, addr, lineRateBps);
      servers.Get (i)->AddApplication (a);
      a->SetStartTime (Seconds (0.0));
      a->SetStopTime  (Seconds (30.0));
      apps[i] = a;
    }

  // ── 调度流量（全部立即入队，DCQCN 控制实际发送速率）──
  Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();
  uint64_t pid = 1;

  if (scenario == "incast")
    {
      uint32_t dst = 0, cnt = 0;
      for (uint32_t s = 1; s < N && cnt < incastFanin; ++s, ++cnt)
        for (uint32_t m = 0; m < 64; ++m)
          Simulator::Schedule (Seconds (1.0), &RoceHost::EnqueueAndSend,
                               apps[s], dst, pktBytes, pid++);
      std::cout << "  Scenario: incast fan-in=" << std::min (incastFanin, N-1) << " -> server 0\n";
    }
  else if (scenario == "allreduce")
    {
      for (uint32_t s = 0; s < N; ++s)
        { uint32_t dst = (s+1)%N;
          for (uint32_t m = 0; m < 64; ++m)
            Simulator::Schedule (Seconds (1.0), &RoceHost::EnqueueAndSend,
                                 apps[s], dst, pktBytes, pid++); }
      std::cout << "  Scenario: allreduce ring (N=" << N << ")\n";
    }
  else if (scenario == "bisection")
    {
      for (uint32_t s = 0; s < N/2; ++s)
        for (uint32_t m = 0; m < 64; ++m)
          Simulator::Schedule (Seconds (1.0), &RoceHost::EnqueueAndSend,
                               apps[s], N/2+s, pktBytes, pid++);
      std::cout << "  Scenario: bisection (first half -> second half)\n";
    }
  else
    {
      double when = 1.0;
      for (uint32_t k = 0; k < uniformFlows; ++k)
        {
          uint32_t s = rng->GetInteger (0, N-1), d = rng->GetInteger (0, N-1);
          if (s == d) continue;
          Simulator::Schedule (Seconds (when), &RoceHost::EnqueueAndSend,
                               apps[s], d, pktBytes, pid++);
          when += 5e-7;
        }
      std::cout << "  Scenario: uniform (" << uniformFlows << " flows)\n";
    }

  double simDuration = 1.5;
  Simulator::Stop (Seconds (simDuration));
  std::cout << "  Running...\n\n";
  Simulator::Run ();
  Simulator::Destroy ();

  // ── 统计 ──
  uint64_t delivered = g_done.size ();
  uint64_t dropped   = (g_sent >= delivered) ? (g_sent - delivered) : 0;
  double rxDurSec    = (g_last_recv > g_first_recv && g_first_recv > 0)
                       ? (g_last_recv - g_first_recv) / 1e9 : 0.0;
  double tputGbps    = rxDurSec > 0
                       ? (delivered * pktBytes * 8.0) / (rxDurSec * 1e9) : 0.0;

  std::vector<double> lats;
  lats.reserve (g_done.size ());
  for (auto &p : g_done) if (p.recvNs > 0) lats.push_back (p.recvNs - p.sentNs);
  std::sort (lats.begin (), lats.end ());
  double meanUs=0, p50Us=0, p99Us=0, maxUs=0;
  if (!lats.empty ())
    {
      double sum=0; for (double v:lats) sum+=v;
      meanUs = sum/lats.size()/1000.0;
      p50Us  = lats[lats.size()*50/100]/1000.0;
      p99Us  = lats[lats.size()*99/100]/1000.0;
      maxUs  = lats.back()/1000.0;
    }

  uint32_t nSwitches   = tors.GetN()+aggrs.GetN()+cores.GetN();
  double P_srv_static  = 2000.0, P_srv_full = 8000.0;
  double P_sw          = 300.0,  E_dyn_bit  = 10e-12;
  double staticEnergy  = (N*P_srv_static + nSwitches*P_sw) * simDuration;
  double computeEnergy = rxDurSec > 0 ? N*(P_srv_full-P_srv_static)*rxDurSec : 0;
  double netEnergy     = g_total_bits * E_dyn_bit;
  double totalEnergy   = staticEnergy + computeEnergy + netEnergy;

  std::cout << "=== Results: scenario=" << scenario << " K=" << K << " N=" << N << " ===\n";
  std::cout << "sent=" << g_sent << "  delivered=" << delivered
            << "  dropped=" << dropped
            << " (" << (g_sent ? 100.0*dropped/g_sent : 0.0) << "%)\n";
  std::cout << "-------------------------------------------\n";
  std::cout << "吞吐量 (Throughput)  : " << tputGbps << " Gbps\n";
  std::cout << "Duration            : " << rxDurSec  << " s\n";
  std::cout << "-------------------------------------------\n";
  std::cout << "延迟 (Latency)\n";
  std::cout << "  mean=" << meanUs << " µs  p50=" << p50Us
            << " µs  p99=" << p99Us << " µs  max=" << maxUs << " µs\n";
  std::cout << "-------------------------------------------\n";
  std::cout << "RoCEv2 / DCQCN 诊断\n";
  std::cout << "  CE 标记包数  : " << g_ce_pkts  << "\n";
  std::cout << "  CNP 发出     : " << g_cnp_sent << "\n";
  std::cout << "  CNP 收到     : " << g_cnp_recv << "\n";
  std::cout << "  CE 率        : "
            << (delivered > 0 ? 100.0*g_ce_pkts/delivered : 0.0) << "%\n";
  std::cout << "-------------------------------------------\n";
  std::cout << "全网总能耗 (Total Energy): " << totalEnergy << " J\n";
  std::cout << "  ├─ 服务器静态     : " << N*P_srv_static*simDuration << " J\n";
  std::cout << "  ├─ 服务器计算增量 : " << computeEnergy << " J\n";
  std::cout << "  ├─ 交换机静态     : " << nSwitches*P_sw*simDuration
            << " J  (" << nSwitches << " × 300W)\n";
  std::cout << "  └─ 链路动态       : " << netEnergy << " J\n";
  std::cout << "Avg Power           : " << totalEnergy/simDuration << " W\n";
  std::cout << "-------------------------------------------\n";

  std::ofstream csv ("fattree_roce_result.csv");
  csv << "scenario,K,N,switches,sent,delivered,dropped,"
         "ce_pkts,cnp_sent,cnp_recv,"
         "throughput_Gbps,mean_us,p99_us,total_energy_J\n";
  csv << scenario << "," << K << "," << N << "," << nSwitches << ","
      << g_sent << "," << delivered << "," << dropped << ","
      << g_ce_pkts << "," << g_cnp_sent << "," << g_cnp_recv << ","
      << tputGbps << "," << meanUs << "," << p99Us << "," << totalEnergy << "\n";
  std::cout << "wrote fattree_roce_result.csv\n";
  return 0;
}

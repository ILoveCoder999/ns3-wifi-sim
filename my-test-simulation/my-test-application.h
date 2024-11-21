/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TUTORIAL_APP_H
#define TUTORIAL_APP_H

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-mpdu.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/wifi-net-device.h"
#include "ns3/phy-entity.h"

/**
 * Tutorial - a simple Application sending packets.
 */
class TestApplication : public ns3::Application
{
    public:
        TestApplication();
        ~TestApplication() override;

        /**
         * Register this type.
         * \return The TypeId.
         */
        static ns3::TypeId GetTypeId();


        struct LatencyCallbackArgs {
            int node_id;
            uint32_t channel_number;
            uint32_t channel_width;
            uint32_t packet_id;
            ns3::Time rtt;
            uint32_t queue_n_packets;
            ns3::SignalNoiseDbm signalNoise;

            friend std::ostream& 
            operator<<(std::ostream& os, const LatencyCallbackArgs& args)
            {
                return os << "LatencyCallbackArgs(node_id=" << args.node_id << ",channel_number=" << args.channel_number << ",channel_width=" << args.channel_width << ", packet_id=" << args.packet_id << ", rtt=" << args.rtt.ToInteger(ns3::Time::Unit::NS) << "ns, n_packets=" << args.queue_n_packets << ", snr=" << args.signalNoise.signal << "/" << args.signalNoise.noise << ")";
            }

            void
            Print(std::ostream& os)
            {
                os << packet_id << " " << channel_number << " " << channel_width << " " << rtt.ToInteger(ns3::Time::Unit::NS) << " " << queue_n_packets << " " << signalNoise.signal << " " << signalNoise.noise << std::endl;
            }
        };

        typedef void (*LatencyCallback) (LatencyCallbackArgs args);

    private:

        struct LowerMac {
        ns3::Ptr<ns3::WifiNetDevice> device;
        ns3::Ptr<ns3::Socket> socket;
        ns3::SignalNoiseDbm signalNoise;
        };

        void StartApplication() override;
        void StopApplication() override;

        /// Send a packet.
        void SendPacket();
        void ReceiveAck(uint32_t i, ns3::Ptr<const ns3::WifiMpdu> mpdu);
        void ReceiveNAck(ns3::Ptr<const ns3::WifiMpdu> mpdu);
        void Timeout(uint8_t reason, ns3::Ptr<const ns3::WifiMpdu> mpdu, const ns3::WifiTxVector& txVector);
        void MonitorRx(uint32_t i, ns3::Ptr<const ns3::Packet> packet,
                                                uint16_t channelFreqMhz,
                                                ns3::WifiTxVector txVector,
                                                ns3::MpduInfo aMpdu,
                                                ns3::SignalNoiseDbm signalNoise,
                                                uint16_t staId);

        int m_id;
        std::vector<LowerMac> m_lmacs;
        ns3::Address m_peerAddress;
        uint32_t m_peerPort;
        ns3::Time m_interval;
        uint32_t m_packetSize;
        uint32_t m_nPackets;
        uint32_t m_burstSize;
        ns3::EventId m_sendEvent;
        uint32_t m_packetsSent;
        ns3::TracedCallback<LatencyCallbackArgs> m_receivedCallback;
};

#endif /* TUTORIAL_APP_H */

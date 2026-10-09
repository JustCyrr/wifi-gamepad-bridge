#pragma once
#include <cstdint>
#include <mutex>

namespace WifiGamepad {

struct MetricsSnapshot {
    double packet_rate_hz;
    double jitter_ms;
    double packet_loss_percent;
    double rtt_ms;
    uint32_t total_packets;
    uint32_t dropped_packets;
    uint32_t out_of_order_packets;
};

class MetricsTracker {
public:
    MetricsTracker();

    void on_input_packet(uint32_t sequence, uint64_t client_timestamp_us, uint64_t recv_time_us);
    void on_ping_pong_rtt(double rtt_ms);
    MetricsSnapshot get_snapshot();

private:
    std::mutex m_mutex;
    
    // Packet rate
    uint64_t m_last_sec_tick_us;
    uint32_t m_packets_this_sec;
    double   m_current_rate_hz;

    // Sequence tracking for loss
    bool     m_has_first_seq;
    uint32_t m_last_sequence;
    uint32_t m_total_received;
    uint32_t m_expected_packets;
    uint32_t m_dropped_packets;
    uint32_t m_out_of_order;

    uint32_t m_dropped_this_sec;
    uint32_t m_expected_this_sec;
    double   m_rolling_loss_percent;

    // Jitter (RFC 3550)
    bool     m_has_jitter_sample;
    int64_t  m_prev_transit_time_us;
    double   m_jitter_us;

    // RTT
    double   m_latest_rtt_ms;
};

} // namespace WifiGamepad

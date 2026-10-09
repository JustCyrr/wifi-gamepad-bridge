#include "metrics.h"
#include <cmath>
#include <chrono>

namespace WifiGamepad {

MetricsTracker::MetricsTracker()
    : m_last_sec_tick_us(0)
    , m_packets_this_sec(0)
    , m_current_rate_hz(0.0)
    , m_has_first_seq(false)
    , m_last_sequence(0)
    , m_total_received(0)
    , m_expected_packets(0)
    , m_dropped_packets(0)
    , m_out_of_order(0)
    , m_has_jitter_sample(false)
    , m_prev_transit_time_us(0)
    , m_jitter_us(0.0)
    , m_latest_rtt_ms(0.0)
{
}

void MetricsTracker::on_input_packet(uint32_t sequence, uint64_t client_timestamp_us, uint64_t recv_time_us) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_total_received++;
    m_packets_this_sec++;

    // 1. Packet Rate (Hz) & Rolling 1-Second Loss
    if (m_last_sec_tick_us == 0) {
        m_last_sec_tick_us = recv_time_us;
    } else {
        uint64_t diff = recv_time_us - m_last_sec_tick_us;
        if (diff >= 1000000) { // 1 second elapsed
            m_current_rate_hz = (double)m_packets_this_sec * 1000000.0 / (double)diff;
            m_packets_this_sec = 0;

            if (m_expected_this_sec > 0) {
                m_rolling_loss_percent = ((double)m_dropped_this_sec / (double)m_expected_this_sec) * 100.0;
            } else {
                m_rolling_loss_percent = 0.0;
            }
            m_dropped_this_sec = 0;
            m_expected_this_sec = 0;
            m_last_sec_tick_us = recv_time_us;
        }
    }

    // 2. Sequence & Loss Tracking
    if (!m_has_first_seq) {
        m_has_first_seq = true;
        m_last_sequence = sequence;
        m_expected_packets = 1;
        m_expected_this_sec = 1;
    } else {
        if (sequence > m_last_sequence) {
            uint32_t gap = sequence - m_last_sequence;
            if (gap > 500) {
                // Client reset sequence / reconnected
                m_last_sequence = sequence;
                m_dropped_this_sec = 0;
                m_expected_this_sec = 1;
            } else {
                if (gap > 1) {
                    m_dropped_packets += (gap - 1);
                    m_dropped_this_sec += (gap - 1);
                }
                m_expected_packets += gap;
                m_expected_this_sec += gap;
                m_last_sequence = sequence;
            }
        } else if (sequence < m_last_sequence) {
            // Out of order packet arrived: compensate drop if counted
            if (m_dropped_packets > 0) {
                m_dropped_packets--;
            }
            if (m_dropped_this_sec > 0) {
                m_dropped_this_sec--;
            }
            m_out_of_order++;
        }
    }

    // 3. Jitter Calculation (RFC 3550 standard)
    if (client_timestamp_us > 0) {
        int64_t transit_us = static_cast<int64_t>(recv_time_us) - static_cast<int64_t>(client_timestamp_us);
        if (!m_has_jitter_sample) {
            m_has_jitter_sample = true;
            m_prev_transit_time_us = transit_us;
            m_jitter_us = 0.0;
        } else {
            int64_t d = transit_us - m_prev_transit_time_us;
            m_prev_transit_time_us = transit_us;
            double abs_d = std::abs(static_cast<double>(d));
            // J(i) = J(i-1) + (|D(i-1, i)| - J(i-1)) / 16
            m_jitter_us += (abs_d - m_jitter_us) / 16.0;
        }
    }
}

void MetricsTracker::on_ping_pong_rtt(double rtt_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_latest_rtt_ms = rtt_ms;
}

MetricsSnapshot MetricsTracker::get_snapshot() {
    std::lock_guard<std::mutex> lock(m_mutex);

    MetricsSnapshot snap;
    snap.packet_rate_hz = m_current_rate_hz;
    snap.jitter_ms = m_jitter_us / 1000.0;
    snap.rtt_ms = m_latest_rtt_ms;
    snap.total_packets = m_total_received;
    snap.dropped_packets = m_dropped_packets;
    snap.out_of_order_packets = m_out_of_order;

    snap.packet_loss_percent = m_rolling_loss_percent;
    return snap;
}

} // namespace WifiGamepad

#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "protocol.h"
#include "vigem_controller.h"
#include "metrics.h"
#include <atomic>
#include <thread>
#include <string>

namespace WifiGamepad {

class UdpReceiver {
public:
    UdpReceiver(ViGEmController& controller, MetricsTracker& metrics, uint32_t pairing_pin);
    ~UdpReceiver();

    bool start(uint16_t port, std::string& error_message);
    void stop();

    bool is_client_connected() const { return m_client_connected; }
    std::string get_client_ip() const {
        std::lock_guard<std::mutex> lock(m_ip_mutex);
        return m_client_ip;
    }

    void send_rumble(uint8_t large_motor, uint8_t small_motor);
    bool get_last_input_packet(InputPacket& out_packet);
    uint32_t get_pairing_pin() const { return m_pairing_pin; }

private:
    void network_worker();
    void discovery_worker();
    uint64_t get_time_us() const;

    ViGEmController& m_controller;
    MetricsTracker&  m_metrics;
    uint32_t         m_pairing_pin;

    SOCKET           m_socket;
    SOCKET           m_discovery_socket;
    std::atomic<bool> m_running;
    std::thread      m_recv_thread;
    std::thread      m_discovery_thread;

    std::atomic<bool> m_client_connected;
    mutable std::mutex m_ip_mutex;
    std::string      m_client_ip;
    uint16_t         m_session_token;
    sockaddr_in      m_client_addr;

    std::mutex       m_packet_mutex;
    InputPacket      m_last_input_packet;
    bool             m_has_packet;
};

} // namespace WifiGamepad

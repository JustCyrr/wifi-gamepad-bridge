#include "udp_receiver.h"
#include <iostream>
#include <chrono>
#include <avrt.h>
#include <timeapi.h>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "winmm.lib")

namespace WifiGamepad {

UdpReceiver::UdpReceiver(ViGEmController& controller, MetricsTracker& metrics, uint32_t pairing_pin)
    : m_controller(controller)
    , m_metrics(metrics)
    , m_pairing_pin(pairing_pin)
    , m_socket(INVALID_SOCKET)
    , m_discovery_socket(INVALID_SOCKET)
    , m_running(false)
    , m_client_connected(false)
    , m_session_token(0x8F4A)
    , m_has_packet(false)
{
    memset(&m_client_addr, 0, sizeof(m_client_addr));
    memset(&m_last_input_packet, 0, sizeof(m_last_input_packet));

    m_controller.set_rumble_callback([this](uint8_t large_motor, uint8_t small_motor) {
        this->send_rumble(large_motor, small_motor);
    });
}

void UdpReceiver::send_rumble(uint8_t large_motor, uint8_t small_motor) {
    if (!m_client_connected || m_socket == INVALID_SOCKET) {
        return;
    }
    sockaddr_in target_addr{};
    {
        std::lock_guard<std::mutex> lock(m_ip_mutex);
        target_addr = m_client_addr;
    }
    if (target_addr.sin_port == 0) return;

    RumblePacket pkt{};
    pkt.magic = PROTOCOL_MAGIC;
    pkt.type = PACKET_TYPE_RUMBLE;
    pkt.large_motor = large_motor;
    pkt.small_motor = small_motor;

    sendto(m_socket, (const char*)&pkt, sizeof(pkt), 0, (sockaddr*)&target_addr, sizeof(target_addr));
}

bool UdpReceiver::get_last_input_packet(InputPacket& out_packet) {
    std::lock_guard<std::mutex> lock(m_packet_mutex);
    if (!m_has_packet) return false;
    out_packet = m_last_input_packet;
    return true;
}

UdpReceiver::~UdpReceiver() {
    stop();
}

uint64_t UdpReceiver::get_time_us() const {
    static LARGE_INTEGER freq;
    static bool init = false;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = true;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>((counter.QuadPart * 1000000ULL) / freq.QuadPart);
}

bool UdpReceiver::start(uint16_t port, std::string& error_message) {
    if (m_running) {
        return true;
    }

    WSADATA wsaData;
    int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_err != 0) {
        error_message = "WSAStartup failed with error: " + std::to_string(wsa_err);
        return false;
    }

    // 1. Create Main Input Socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        error_message = "Failed to create UDP socket: " + std::to_string(WSAGetLastError());
        WSACleanup();
        return false;
    }

    // Socket Opt: Low buffer size to eliminate OS queuing latency
    int rcvbuf = 8192;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf));

    // Socket Opt: Fix Windows WSAECONNRESET bug on UDP
    BOOL bNewBehavior = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(m_socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(m_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        error_message = "Failed to bind UDP socket on port " + std::to_string(port) + ": " + std::to_string(WSAGetLastError());
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    // 2. Create Discovery Socket (Port 52151 Broadcast)
    m_discovery_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_discovery_socket != INVALID_SOCKET) {
        BOOL reuse = TRUE;
        setsockopt(m_discovery_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        sockaddr_in disc_addr{};
        disc_addr.sin_family = AF_INET;
        disc_addr.sin_addr.s_addr = INADDR_ANY;
        disc_addr.sin_port = htons(DISCOVERY_PORT);
        bind(m_discovery_socket, (sockaddr*)&disc_addr, sizeof(disc_addr));
    }

    timeBeginPeriod(1); // Set Windows timer precision to 1 ms

    m_running = true;
    m_recv_thread = std::thread(&UdpReceiver::network_worker, this);
    if (m_discovery_socket != INVALID_SOCKET) {
        m_discovery_thread = std::thread(&UdpReceiver::discovery_worker, this);
    }

    return true;
}

void UdpReceiver::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_discovery_socket != INVALID_SOCKET) {
        closesocket(m_discovery_socket);
        m_discovery_socket = INVALID_SOCKET;
    }

    if (m_recv_thread.joinable()) {
        m_recv_thread.join();
    }
    if (m_discovery_thread.joinable()) {
        m_discovery_thread.join();
    }

    timeEndPeriod(1);
    WSACleanup();
}

void UdpReceiver::network_worker() {
    // Elevate thread to Multimedia / Time Critical for minimum jitter
    DWORD taskIndex = 0;
    HANDLE hAvrt = AvSetMmThreadCharacteristicsW(L"Games", &taskIndex);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    uint8_t buffer[256];
    sockaddr_in sender_addr;
    int sender_len = sizeof(sender_addr);

    while (m_running) {
        int sender_len = sizeof(sender_addr);
        int bytes = recvfrom(m_socket, (char*)buffer, sizeof(buffer), 0, (sockaddr*)&sender_addr, &sender_len);
        if (bytes <= 0) {
            continue;
        }

        uint64_t now_us = get_time_us();

        if (buffer[0] != PROTOCOL_MAGIC) {
            continue;
        }

        uint8_t packet_type = buffer[1];

        if (packet_type == PACKET_TYPE_INPUT && bytes >= sizeof(InputPacket)) {
            const InputPacket* packet = reinterpret_cast<const InputPacket*>(buffer);

            // Forward to ViGEm virtual controller immediately
            m_controller.update(*packet);

            {
                std::lock_guard<std::mutex> lock(m_packet_mutex);
                m_last_input_packet = *packet;
                m_has_packet = true;
            }

            // Record metrics
            m_metrics.on_input_packet(packet->sequence, packet->timestamp_us, now_us);

            {
                std::lock_guard<std::mutex> lock(m_ip_mutex);
                m_client_addr = sender_addr;
                if (!m_client_connected) {
                    char ip_buf[INET_ADDRSTRLEN] = {0};
                    inet_ntop(AF_INET, &(sender_addr.sin_addr), ip_buf, INET_ADDRSTRLEN);
                    m_client_ip = ip_buf;
                    m_client_connected = true;
                }
            }
        }
        else if (packet_type == PACKET_TYPE_PING && bytes >= sizeof(PingPacket)) {
            PingPacket* ping = reinterpret_cast<PingPacket*>(buffer);
            ping->type = PACKET_TYPE_PONG;
            ping->server_timestamp = now_us;

            // Reflect pong immediately for client RTT calculation
            sendto(m_socket, (const char*)ping, sizeof(PingPacket), 0, (sockaddr*)&sender_addr, sender_len);
        }
        else if (packet_type == PACKET_TYPE_PAIR_REQ && bytes >= sizeof(PairRequestPacket)) {
            const PairRequestPacket* req = reinterpret_cast<const PairRequestPacket*>(buffer);
            PairResponsePacket resp{};
            resp.magic = PROTOCOL_MAGIC;
            resp.type = PACKET_TYPE_PAIR_RESP;

            if (req->pin_code == m_pairing_pin || m_pairing_pin == 0) {
                resp.success = 1;
                resp.session_token = m_session_token;
                m_client_connected = true;
                char ip_buf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(sender_addr.sin_addr), ip_buf, INET_ADDRSTRLEN);
                m_client_ip = ip_buf;
                m_client_addr = sender_addr;
            } else {
                resp.success = 0;
                resp.session_token = 0;
            }

            sendto(m_socket, (const char*)&resp, sizeof(resp), 0, (sockaddr*)&sender_addr, sender_len);
        }
        else if (packet_type == PACKET_TYPE_DISCONNECT) {
            m_client_connected = false;
            m_client_ip = "";
        }
    }

    if (hAvrt) {
        AvRevertMmThreadCharacteristics(hAvrt);
    }
}

void UdpReceiver::discovery_worker() {
    uint8_t buffer[128];
    sockaddr_in sender_addr;
    int sender_len = sizeof(sender_addr);

    char hostname[32] = {0};
    gethostname(hostname, sizeof(hostname) - 1);

    while (m_running) {
        int bytes = recvfrom(m_discovery_socket, (char*)buffer, sizeof(buffer), 0, (sockaddr*)&sender_addr, &sender_len);
        if (bytes <= 0) {
            continue;
        }

        if (buffer[0] == PROTOCOL_MAGIC && buffer[1] == PACKET_TYPE_DISCOVERY) {
            DiscoveryPacket ack{};
            ack.magic = PROTOCOL_MAGIC;
            ack.type = PACKET_TYPE_DISCOVERY_ACK;
            ack.port = DEFAULT_SERVER_PORT;
            strncpy_s(ack.hostname, sizeof(ack.hostname), hostname, sizeof(ack.hostname) - 1);
            ack.is_paired = m_client_connected ? 1 : 0;

            sendto(m_discovery_socket, (const char*)&ack, sizeof(ack), 0, (sockaddr*)&sender_addr, sender_len);
        }
    }
}

} // namespace WifiGamepad

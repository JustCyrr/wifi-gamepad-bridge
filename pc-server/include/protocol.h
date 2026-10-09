#pragma once
#include <cstdint>

#pragma pack(push, 1)

namespace WifiGamepad {

constexpr uint8_t PROTOCOL_MAGIC = 0x57; // 'W' (WiFi Gamepad)
constexpr uint16_t DEFAULT_SERVER_PORT = 52150;
constexpr uint16_t DISCOVERY_PORT = 52151;

enum PacketType : uint8_t {
    PACKET_TYPE_INPUT        = 0x01,
    PACKET_TYPE_PING         = 0x02,
    PACKET_TYPE_PONG         = 0x03,
    PACKET_TYPE_DISCOVERY    = 0x04,
    PACKET_TYPE_DISCOVERY_ACK= 0x05,
    PACKET_TYPE_PAIR_REQ     = 0x06,
    PACKET_TYPE_PAIR_RESP    = 0x07,
    PACKET_TYPE_DISCONNECT   = 0x08,
    PACKET_TYPE_RUMBLE       = 0x09
};

// 16-bit Button Bitmask (Directly maps to XInput XINPUT_GAMEPAD buttons)
enum GamepadButtons : uint16_t {
    BTN_DPAD_UP        = 0x0001,
    BTN_DPAD_DOWN      = 0x0002,
    BTN_DPAD_LEFT      = 0x0004,
    BTN_DPAD_RIGHT     = 0x0008,
    BTN_START          = 0x0010,
    BTN_BACK           = 0x0020,
    BTN_LEFT_THUMB     = 0x0040, // L3
    BTN_RIGHT_THUMB    = 0x0080, // R3
    BTN_LEFT_SHOULDER  = 0x0100, // LB
    BTN_RIGHT_SHOULDER = 0x0200, // RB
    BTN_GUIDE          = 0x0400, // Xbox Home / Guide
    BTN_A              = 0x1000,
    BTN_B              = 0x2000,
    BTN_X              = 0x4000,
    BTN_Y              = 0x8000
};

// Input Packet: Exactly 28 bytes binary payload
struct InputPacket {
    uint8_t  magic;             // 0x00: 0x57
    uint8_t  type;              // 0x01: PACKET_TYPE_INPUT (0x01)
    uint16_t session_token;     // 0x02: Auth token from pairing
    uint32_t sequence;          // 0x04: Monotonic sequence number
    uint64_t timestamp_us;      // 0x08: Client hardware event timestamp (microseconds)
    uint16_t buttons;           // 0x10: GamepadButtons bitmask
    uint8_t  trigger_left;      // 0x12: 0 - 255 (LT)
    uint8_t  trigger_right;     // 0x13: 0 - 255 (RT)
    int16_t  thumb_lx;          // 0x14: -32768 to 32767
    int16_t  thumb_ly;          // 0x16: -32768 to 32767
    int16_t  thumb_rx;          // 0x18: -32768 to 32767
    int16_t  thumb_ry;          // 0x1A: -32768 to 32767
};
static_assert(sizeof(InputPacket) == 28, "InputPacket must be exactly 28 bytes");

// Ping / Pong Packet for RTT and Clock Synchronization (20 bytes)
struct PingPacket {
    uint8_t  magic;             // 0x57
    uint8_t  type;              // PACKET_TYPE_PING or PACKET_TYPE_PONG
    uint16_t session_token;
    uint32_t sequence;
    uint64_t client_timestamp;  // Sent by client
    uint64_t server_timestamp;  // Filled by server upon receipt
};
static_assert(sizeof(PingPacket) == 24, "PingPacket must be 24 bytes");

// Rumble Packet: Sent from PC to Android for Force Feedback / Vibration
struct RumblePacket {
    uint8_t magic;             // 0x57
    uint8_t type;              // PACKET_TYPE_RUMBLE (0x09)
    uint8_t large_motor;       // 0 - 255 (Heavy rumble)
    uint8_t small_motor;       // 0 - 255 (Light rumble)
};
static_assert(sizeof(RumblePacket) == 4, "RumblePacket must be 4 bytes");

// Discovery Probe / Announce (Broadcast UDP)
struct DiscoveryPacket {
    uint8_t  magic;             // 0x57
    uint8_t  type;              // PACKET_TYPE_DISCOVERY or PACKET_TYPE_DISCOVERY_ACK
    uint16_t port;              // Server listening port (52150)
    char     hostname[32];      // PC Hostname (null-terminated)
    uint8_t  is_paired;         // 1 if already paired with client
};

// Pairing Request / Response
struct PairRequestPacket {
    uint8_t  magic;
    uint8_t  type;              // PACKET_TYPE_PAIR_REQ
    uint32_t pin_code;          // 6-digit PIN entered on Android (e.g. 482193)
    char     device_id[32];     // Unique Android device UUID
};

struct PairResponsePacket {
    uint8_t  magic;
    uint8_t  type;              // PACKET_TYPE_PAIR_RESP
    uint8_t  success;           // 1 = OK, 0 = Invalid PIN
    uint16_t session_token;     // Generated session token if OK
};

} // namespace WifiGamepad

#pragma pack(pop)

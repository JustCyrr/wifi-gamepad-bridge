#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "protocol.h"
#include <ViGEm/Client.h>
#include <atomic>
#include <string>
#include <functional>

namespace WifiGamepad {

using RumbleCallback = std::function<void(uint8_t large_motor, uint8_t small_motor)>;

class ViGEmController {
public:
    ViGEmController();
    ~ViGEmController();

    bool initialize(std::string& error_message);
    void update(const InputPacket& packet);
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    void set_rumble_callback(RumbleCallback cb) { m_rumble_cb = cb; }

    friend VOID CALLBACK x360_notification_handler(
        PVIGEM_CLIENT Client,
        PVIGEM_TARGET Target,
        UCHAR LargeMotor,
        UCHAR SmallMotor,
        UCHAR LedNumber,
        LPVOID UserData
    );

private:
    PVIGEM_CLIENT m_client;
    PVIGEM_TARGET m_target;
    std::atomic<bool> m_initialized;
    RumbleCallback m_rumble_cb;
};

} // namespace WifiGamepad

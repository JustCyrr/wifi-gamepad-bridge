#include "vigem_controller.h"
#include <iostream>

namespace WifiGamepad {

ViGEmController::ViGEmController()
    : m_client(nullptr)
    , m_target(nullptr)
    , m_initialized(false)
{
}

ViGEmController::~ViGEmController() {
    shutdown();
}

VOID CALLBACK x360_notification_handler(
    PVIGEM_CLIENT Client,
    PVIGEM_TARGET Target,
    UCHAR LargeMotor,
    UCHAR SmallMotor,
    UCHAR LedNumber,
    LPVOID UserData
) {
    (void)Client;
    (void)Target;
    (void)LedNumber;
    auto self = reinterpret_cast<ViGEmController*>(UserData);
    if (self && self->m_rumble_cb) {
        self->m_rumble_cb(static_cast<uint8_t>(LargeMotor), static_cast<uint8_t>(SmallMotor));
    }
}

bool ViGEmController::initialize(std::string& error_message) {
    if (m_initialized) {
        return true;
    }

    m_client = vigem_alloc();
    if (!m_client) {
        error_message = "Failed to allocate ViGEmClient memory.";
        return false;
    }

    const auto connect_res = vigem_connect(m_client);
    if (!VIGEM_SUCCESS(connect_res)) {
        error_message = "Cannot connect to ViGEmBus driver. Error code: " + std::to_string(connect_res) +
                        "\nPlease verify that ViGEmBus driver is installed.";
        vigem_free(m_client);
        m_client = nullptr;
        return false;
    }

    m_target = vigem_target_x360_alloc();
    if (!m_target) {
        error_message = "Failed to allocate ViGEm X360 target device.";
        vigem_disconnect(m_client);
        vigem_free(m_client);
        m_client = nullptr;
        return false;
    }

    const auto add_res = vigem_target_add(m_client, m_target);
    if (!VIGEM_SUCCESS(add_res)) {
        error_message = "Failed to plugin virtual Xbox 360 controller. Error code: " + std::to_string(add_res);
        vigem_target_free(m_target);
        m_target = nullptr;
        vigem_disconnect(m_client);
        vigem_free(m_client);
        m_client = nullptr;
        return false;
    }

    // Register rumble / vibration feedback notification callback
    vigem_target_x360_register_notification(m_client, m_target, &x360_notification_handler, this);

    m_initialized = true;
    return true;
}

void ViGEmController::update(const InputPacket& packet) {
    if (!m_initialized || !m_client || !m_target) {
        return;
    }

    XUSB_REPORT report;
    report.wButtons      = packet.buttons;
    report.bLeftTrigger  = packet.trigger_left;
    report.bRightTrigger = packet.trigger_right;
    report.sThumbLX      = packet.thumb_lx;
    report.sThumbLY      = packet.thumb_ly;
    report.sThumbRX      = packet.thumb_rx;
    report.sThumbRY      = packet.thumb_ry;

    vigem_target_x360_update(m_client, m_target, report);
}

void ViGEmController::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_initialized = false;

    if (m_client && m_target) {
        vigem_target_remove(m_client, m_target);
        vigem_target_free(m_target);
        m_target = nullptr;
    }

    if (m_client) {
        vigem_disconnect(m_client);
        vigem_free(m_client);
        m_client = nullptr;
    }
}

} // namespace WifiGamepad

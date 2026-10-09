#include "vigem_controller.h"
#include "udp_receiver.h"
#include "metrics.h"
#include "gui_window.h"
#include <iostream>
#include <iomanip>
#include <random>
#include <csignal>
#include <atomic>

uint32_t generate_pairing_pin() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(100000, 999999);
    return dis(gen);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 1. Initialize ViGEm Virtual Xbox Controller
    WifiGamepad::ViGEmController controller;
    std::string vigem_err;
    if (!controller.initialize(vigem_err)) {
        MessageBoxA(NULL, vigem_err.c_str(), "ViGEmBus Driver Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // 2. Metrics & Pairing PIN
    WifiGamepad::MetricsTracker metrics;
    uint32_t pairing_pin = generate_pairing_pin();

    // 3. Start UDP Receiver
    WifiGamepad::UdpReceiver receiver(controller, metrics, pairing_pin);
    std::string net_err;
    if (!receiver.start(WifiGamepad::DEFAULT_SERVER_PORT, net_err)) {
        MessageBoxA(NULL, net_err.c_str(), "Network Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // 4. Create and run Modern Dark GUI Dashboard
    WifiGamepad::GuiWindow gui(receiver, metrics, controller);
    if (!gui.create(hInstance, nCmdShow)) {
        MessageBoxA(NULL, "Failed to create GUI Window.", "GUI Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    int ret = gui.run_message_loop();

    receiver.stop();
    controller.shutdown();
    return ret;
}

// Fallback main for console builds
int main() {
    return WinMain(GetModuleHandle(NULL), NULL, GetCommandLineA(), SW_SHOW);
}


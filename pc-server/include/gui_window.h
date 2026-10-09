#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include "udp_receiver.h"
#include "metrics.h"
#include "vigem_controller.h"

namespace WifiGamepad {

class GuiWindow {
public:
    GuiWindow(UdpReceiver& receiver, MetricsTracker& metrics, ViGEmController& controller);
    ~GuiWindow();

    bool create(HINSTANCE hInstance, int nCmdShow);
    int run_message_loop();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void on_paint(HWND hWnd);
    void draw_dashboard(Gdiplus::Graphics& g, int width, int height);
    void enable_blur_behind(HWND hWnd);

    UdpReceiver&     m_receiver;
    MetricsTracker&  m_metrics;
    ViGEmController& m_controller;

    HWND            m_hwnd;
    ULONG_PTR       m_gdiplus_token;
    NOTIFYICONDATAW m_nid;

    // Double buffering
    HDC             m_mem_dc;
    HBITMAP         m_mem_bitmap;
    int             m_mem_width;
    int             m_mem_height;

    RECT            m_rumble_btn_rc;
};

} // namespace WifiGamepad

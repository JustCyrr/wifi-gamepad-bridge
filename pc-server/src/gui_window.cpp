#include "gui_window.h"
#include "resource.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <thread>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#define WM_TRAYICON     (WM_USER + 101)
#define ID_TRAY_SHOW    2001
#define ID_TRAY_RUMBLE  2002
#define ID_TRAY_EXIT    2003

using namespace Gdiplus;

namespace WifiGamepad {

// Helper: Add rounded rectangle to a GraphicsPath
static void AddRoundedRect(GraphicsPath& path, RectF rc, float r) {
    float d = r * 2.0f;
    path.AddArc(rc.X, rc.Y, d, d, 180, 90);
    path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270, 90);
    path.AddArc(rc.X + rc.Width - d, rc.Y + rc.Height - d, d, d, 0, 90);
    path.AddArc(rc.X, rc.Y + rc.Height - d, d, d, 90, 90);
    path.CloseFigure();
}

GuiWindow::GuiWindow(UdpReceiver& receiver, MetricsTracker& metrics, ViGEmController& controller)
    : m_receiver(receiver)
    , m_metrics(metrics)
    , m_controller(controller)
    , m_hwnd(NULL)
    , m_gdiplus_token(0)
    , m_mem_dc(NULL)
    , m_mem_bitmap(NULL)
    , m_mem_width(0)
    , m_mem_height(0)
{
    memset(&m_nid, 0, sizeof(m_nid));
    m_rumble_btn_rc = {0, 0, 0, 0};

    GdiplusStartupInput gdiInput;
    GdiplusStartup(&m_gdiplus_token, &gdiInput, NULL);
}

GuiWindow::~GuiWindow() {
    if (m_hwnd && m_nid.cbSize) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }
    if (m_mem_bitmap) DeleteObject(m_mem_bitmap);
    if (m_mem_dc) DeleteDC(m_mem_dc);
    if (m_gdiplus_token) {
        GdiplusShutdown(m_gdiplus_token);
    }
}

void GuiWindow::enable_blur_behind(HWND hWnd) {
    // 1. Windows 11 Acrylic Backdrop
    int backdrop = 3; // DWMSBT_TRANSIENTWINDOW (Acrylic Blur)
    DwmSetWindowAttribute(hWnd, 38, &backdrop, sizeof(backdrop));

    // 2. Windows 10 SetWindowCompositionAttribute Acrylic
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        struct ACCENT_POLICY {
            int AccentState;
            int AccentFlags;
            int GradientColor;
            int AnimationId;
        };
        struct WINCOMPATTRDATA {
            int nAttr;
            PVOID pData;
            ULONG ulDataSize;
        };
        typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINCOMPATTRDATA*);
        auto setAttr = (pfnSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
        if (setAttr) {
            ACCENT_POLICY policy{};
            policy.AccentState = 4; // ACCENT_ENABLE_ACRYLICBLURBEHIND
            policy.AccentFlags = 2; // All borders
            policy.GradientColor = 0xBB02142E; // Translucent deep cyan-ocean glass tint
            WINCOMPATTRDATA data{};
            data.nAttr = 19; // WCA_ACCENT_POLICY
            data.pData = &policy;
            data.ulDataSize = sizeof(policy);
            setAttr(hWnd, &data);
        }
    }

    // 3. Extend Frame
    MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hWnd, &margins);
}

LRESULT CALLBACK GuiWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GuiWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        self = reinterpret_cast<GuiWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<GuiWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self) {
        switch (msg) {
        case WM_SYSCOMMAND: {
            // When minimized: Hide from taskbar and minimize to System Tray!
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
            break;
        }
        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hWnd, SW_SHOW);
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            } else if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Show Window");
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_RUMBLE, L"⚡ Test Vibration");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

                SetForegroundWindow(hWnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);

                if (cmd == ID_TRAY_SHOW) {
                    ShowWindow(hWnd, SW_SHOW);
                    ShowWindow(hWnd, SW_RESTORE);
                    SetForegroundWindow(hWnd);
                } else if (cmd == ID_TRAY_RUMBLE) {
                    std::thread([self]() {
                        self->m_receiver.send_rumble(255, 255);
                        Sleep(1500);
                        self->m_receiver.send_rumble(0, 0);
                    }).detach();
                } else if (cmd == ID_TRAY_EXIT) {
                    DestroyWindow(hWnd);
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt{LOWORD(lParam), HIWORD(lParam)};
            if (PtInRect(&self->m_rumble_btn_rc, pt)) {
                // High-power 1.5s rumble test
                std::thread([self]() {
                    self->m_receiver.send_rumble(255, 255);
                    Sleep(1500);
                    self->m_receiver.send_rumble(0, 0);
                }).detach();
            }
            return 0;
        }
        case WM_PAINT:
            self->on_paint(hWnd);
            return 0;
        case WM_TIMER:
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool GuiWindow::create(HINSTANCE hInstance, int nCmdShow) {
    HICON hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!hAppIcon) {
        hAppIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = GuiWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = hAppIcon;
    wc.hIconSm = hAppIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"WiFiGamepadBridgeGUI";

    RegisterClassExW(&wc);

    int w = 880;
    int h = 680;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    m_hwnd = CreateWindowExW(
        0,
        L"WiFiGamepadBridgeGUI",
        L"Wi-Fi Gamepad Bridge",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (screen_w - w) / 2, (screen_h - h) / 2,
        w, h,
        NULL, NULL, hInstance, this
    );

    if (!m_hwnd) return false;

    SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hAppIcon);
    SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hAppIcon);

    // Enable Real Windows Acrylic / Blur Behind
    enable_blur_behind(m_hwnd);

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    int cornerPref = 2; // Rounded corners
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    // Initialize System Tray Icon
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = hAppIcon;
    wcscpy_s(m_nid.szTip, L"Wi-Fi Gamepad Bridge (Active)");
    Shell_NotifyIconW(NIM_ADD, &m_nid);

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    SetTimer(m_hwnd, 1, 16, NULL);
    return true;
}

int GuiWindow::run_message_loop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

void GuiWindow::on_paint(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc;
    GetClientRect(hWnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    if (!m_mem_dc) {
        m_mem_dc = CreateCompatibleDC(hdc);
    }
    if (!m_mem_bitmap || m_mem_width != width || m_mem_height != height) {
        if (m_mem_bitmap) DeleteObject(m_mem_bitmap);
        m_mem_bitmap = CreateCompatibleBitmap(hdc, width, height);
        m_mem_width = width;
        m_mem_height = height;
        SelectObject(m_mem_dc, m_mem_bitmap);
    }

    {
        Graphics g(m_mem_dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        draw_dashboard(g, width, height);
    }

    BitBlt(hdc, 0, 0, width, height, m_mem_dc, 0, 0, SRCCOPY);
    EndPaint(hWnd, &ps);
}

// Draw a Frutiger Aero Crystal Glass Card with specular gloss reflection
static void DrawAeroGlassCard(Graphics& g, RectF rc, Color topColor, Color botColor, Color borderColor, float radius = 18.0f) {
    GraphicsPath path;
    AddRoundedRect(path, rc, radius);

    // 1. Outer Soft Glow Halo
    Pen haloPen(Color(60, borderColor.GetR(), borderColor.GetG(), borderColor.GetB()), 4.0f);
    g.DrawPath(&haloPen, &path);

    // 2. Interior Crystal Gradient
    LinearGradientBrush bodyBrush(rc, topColor, botColor, LinearGradientModeVertical);
    g.FillPath(&bodyBrush, &path);

    // 3. Crisp Glowing Rim
    Pen rimPen(borderColor, 1.5f);
    g.DrawPath(&rimPen, &path);

    // 4. Frutiger Aero Curved Specular Gloss Highlight (Signature 2000s sheen!)
    float glossH = rc.Height * 0.42f;
    RectF glossRc(rc.X + 2.0f, rc.Y + 2.0f, rc.Width - 4.0f, glossH);

    GraphicsPath glossPath;
    glossPath.AddArc(glossRc.X, glossRc.Y, radius * 1.5f, radius * 1.5f, 180, 90);
    glossPath.AddArc(glossRc.X + glossRc.Width - radius * 1.5f, glossRc.Y, radius * 1.5f, radius * 1.5f, 270, 90);
    glossPath.AddLine(glossRc.X + glossRc.Width, glossRc.Y + glossH * 0.7f, glossRc.X, glossRc.Y + glossH);
    glossPath.CloseFigure();

    LinearGradientBrush glossBrush(
        PointF(glossRc.X, glossRc.Y),
        PointF(glossRc.X, glossRc.Y + glossH),
        Color(70, 255, 255, 255), // Bright white sheen
        Color(0, 255, 255, 255)   // Fades to transparent
    );
    g.FillPath(&glossBrush, &glossPath);
}

void GuiWindow::draw_dashboard(Graphics& g, int width, int height) {
    // 1. Semi-Transparent Deep Oceanic Backdrop (Allows desktop blur behind!)
    RectF fullRc(0.0f, 0.0f, (float)width, (float)height);
    LinearGradientBrush bgBrush(
        fullRc,
        Color(190, 4, 30, 68),  // Luminous deep oceanic blue
        Color(230, 2, 12, 28),  // Deep midnight navy
        LinearGradientModeVertical
    );
    g.FillRectangle(&bgBrush, fullRc);

    // Ambient Ethereal Light Trail across top (like PRYDA graphic)
    Pen trailPen(Color(120, 0, 229, 255), 1.5f);
    g.DrawBezier(&trailPen, PointF(0.0f, 60.0f), PointF(width * 0.3f, 40.0f), PointF(width * 0.7f, 80.0f), PointF((float)width, 50.0f));

    FontFamily ff(L"Segoe UI");
    Font fontTitle(&ff, 18.0f, FontStyleBold, UnitPixel);
    Font fontSub(&ff, 11.0f, FontStyleRegular, UnitPixel);
    Font fontBold(&ff, 12.0f, FontStyleBold, UnitPixel);
    Font fontNormal(&ff, 10.5f, FontStyleRegular, UnitPixel);
    Font fontLarge(&ff, 21.0f, FontStyleBold, UnitPixel);

    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);

    StringFormat sfLeft;
    sfLeft.SetAlignment(StringAlignmentNear);
    sfLeft.SetLineAlignment(StringAlignmentCenter);

    // 2. Header
    SolidBrush whiteBrush(Color(255, 240, 249, 255));
    SolidBrush skyBrush(Color(255, 0, 229, 255));
    SolidBrush subBrush(Color(220, 186, 230, 253));

    g.DrawString(L"Wi-Fi Gamepad Bridge", -1, &fontTitle, PointF(28.0f, 16.0f), &whiteBrush);
    g.DrawString(L"Ultra-Low Latency Wireless Gamepad Bridge", -1, &fontSub, PointF(28.0f, 42.0f), &skyBrush);

    // 3. Status & PIN Aero Glass Card
    bool connected = m_receiver.is_client_connected();
    RectF cardStatus(24.0f, 66.0f, (float)(width - 48), 64.0f);
    DrawAeroGlassCard(g, cardStatus, Color(140, 12, 42, 82), Color(160, 5, 18, 40), Color(200, 0, 229, 255), 16.0f);

    float dotX = 44.0f;
    float dotY = 88.0f;
    Color statusColor = connected ? Color(255, 0, 230, 118) : Color(255, 244, 63, 94);
    SolidBrush dotBrush(statusColor);
    g.FillEllipse(&dotBrush, dotX, dotY, 12.0f, 12.0f);

    SolidBrush statusTextBrush(statusColor);
    if (connected) {
        std::string ip = m_receiver.get_client_ip();
        std::wstring ip_w(ip.begin(), ip.end());
        std::wstring stat = L"CONNECTED  (" + (ip_w.empty() ? L"Client" : ip_w) + L")  •  125 Hz Active";
        g.DrawString(stat.c_str(), -1, &fontBold, PointF(dotX + 20.0f, dotY - 2.0f), &statusTextBrush);
    } else {
        g.DrawString(L"WAITING FOR CLIENT CONNECTION...", -1, &fontBold, PointF(dotX + 20.0f, dotY - 2.0f), &statusTextBrush);
    }
    g.DrawString(L"UDP Port: 52150  •  QoS: Best Effort  •  Driver: ViGEmBus Emulation", -1, &fontNormal, PointF(44.0f, 108.0f), &subBrush);

    // "⚡ Test Vibration (1.5s)" Violet Gel Button
    RectF rumbleBtnRc((float)(width - 390), 76.0f, 185.0f, 44.0f);
    m_rumble_btn_rc = { (LONG)rumbleBtnRc.X, (LONG)rumbleBtnRc.Y, (LONG)(rumbleBtnRc.X + rumbleBtnRc.Width), (LONG)(rumbleBtnRc.Y + rumbleBtnRc.Height) };
    DrawAeroGlassCard(g, rumbleBtnRc, Color(220, 147, 51, 234), Color(240, 107, 33, 168), Color(255, 233, 213, 255), 12.0f);
    g.DrawString(L"⚡ Test Vibration (1.5s)", -1, &fontBold, rumbleBtnRc, &sfCenter, &whiteBrush);

    // Pairing PIN Amber / Gold Gel Badge
    RectF pinRc((float)(width - 190), 76.0f, 150.0f, 44.0f);
    DrawAeroGlassCard(g, pinRc, Color(220, 245, 158, 11), Color(240, 180, 83, 9), Color(255, 254, 243, 199), 12.0f);
    std::wstring pinStr = L"PIN: " + std::to_wstring(m_receiver.get_pairing_pin());
    g.DrawString(pinStr.c_str(), -1, &fontBold, pinRc, &sfCenter, &whiteBrush);

    // 4. Live Telemetry Cards (4 elegant frosted glass pills)
    auto snap = m_metrics.get_snapshot();
    float meterCardY = 140.0f;
    float cardW = (float)(width - 48 - 36) / 4.0f;

    auto drawMetricCard = [&](int idx, const wchar_t* title, const std::wstring& val, Color valCol) {
        RectF cRc(24.0f + idx * (cardW + 12.0f), meterCardY, cardW, 64.0f);
        DrawAeroGlassCard(g, cRc, Color(130, 10, 36, 72), Color(150, 4, 16, 36), Color(180, 0, 229, 255), 14.0f);

        RectF titleRc(cRc.X, cRc.Y + 8.0f, cRc.Width, 16.0f);
        g.DrawString(title, -1, &fontNormal, titleRc, &sfCenter, &subBrush);

        SolidBrush valBrush(valCol);
        RectF valRc(cRc.X, cRc.Y + 26.0f, cRc.Width, 30.0f);
        g.DrawString(val.c_str(), -1, &fontLarge, valRc, &sfCenter, &valBrush);
    };

    std::wstringstream ss_rate, ss_jitter, ss_loss, ss_packets;
    ss_rate << std::fixed << std::setprecision(1) << snap.packet_rate_hz << L" Hz";
    ss_jitter << std::fixed << std::setprecision(2) << snap.jitter_ms << L" ms";
    ss_loss << std::fixed << std::setprecision(2) << snap.packet_loss_percent << L" %";
    ss_packets << snap.total_packets;

    drawMetricCard(0, L"POLLING RATE", ss_rate.str(), Color(255, 0, 230, 118));
    drawMetricCard(1, L"NETWORK JITTER", ss_jitter.str(), Color(255, 0, 229, 255));
    drawMetricCard(2, L"PACKET LOSS", ss_loss.str(), snap.packet_loss_percent > 3.0 ? Color(255, 244, 63, 94) : Color(255, 0, 230, 118));
    drawMetricCard(3, L"TOTAL PACKETS", ss_packets.str(), Color(255, 241, 245, 249));

    // 5. Stylized Ergonomic Xbox 360 Controller Container
    RectF cardVis(24.0f, 216.0f, (float)(width - 48), (float)(height - 232));
    DrawAeroGlassCard(g, cardVis, Color(120, 8, 30, 64), Color(140, 3, 12, 28), Color(200, 0, 229, 255), 18.0f);

    RectF visTitleRc(cardVis.X + 20.0f, cardVis.Y + 12.0f, 400.0f, 20.0f);
    g.DrawString(L"LIVE CONTROLLER MONITOR", -1, &fontBold, visTitleRc, &sfLeft, &skyBrush);

    InputPacket pkt{};
    bool has_input = m_receiver.get_last_input_packet(pkt);

    // Realistic Ergonomic Xbox Controller Silhouette Path
    float ctrlW = std::min((float)(width - 180), 680.0f);
    float ctrlH = cardVis.Height - 56.0f;
    float ctrlX = cardVis.X + (cardVis.Width - ctrlW) / 2.0f;
    float ctrlY = cardVis.Y + 40.0f;
    RectF gpBody(ctrlX, ctrlY, ctrlW, ctrlH);

    GraphicsPath gpPath;
    AddRoundedRect(gpPath, gpBody, 36.0f);

    // Controller chassis translucent fill
    LinearGradientBrush gpBrush(gpBody, Color(160, 14, 46, 88), Color(190, 6, 20, 44), LinearGradientModeVertical);
    g.FillPath(&gpBrush, &gpPath);
    Pen gpPen(Color(220, 0, 229, 255), 1.8f);
    g.DrawPath(&gpPen, &gpPath);

    // Upper specular shine across controller
    RectF gpGlossRc(gpBody.X + 4.0f, gpBody.Y + 2.0f, gpBody.Width - 8.0f, gpBody.Height * 0.35f);
    LinearGradientBrush gpGlossBrush(
        PointF(gpGlossRc.X, gpGlossRc.Y),
        PointF(gpGlossRc.X, gpGlossRc.Y + gpGlossRc.Height),
        Color(70, 255, 255, 255),
        Color(0, 255, 255, 255)
    );
    GraphicsPath gpGlossPath;
    AddRoundedRect(gpGlossPath, gpGlossRc, 24.0f);
    g.FillPath(&gpGlossBrush, &gpGlossPath);

    // Center Xbox Guide Orb
    float centerX = gpBody.X + gpBody.Width / 2.0f;
    float centerY = gpBody.Y + 54.0f;
    Color guideCol = (pkt.buttons & BTN_GUIDE) ? Color(255, 0, 230, 118) : Color(255, 0, 176, 255);
    SolidBrush guideBrush(guideCol);
    g.FillEllipse(&guideBrush, centerX - 18.0f, centerY - 18.0f, 36.0f, 36.0f);
    Pen guidePen(Color(255, 255, 255, 255), 1.5f);
    g.DrawEllipse(&guidePen, centerX - 18.0f, centerY - 18.0f, 36.0f, 36.0f);

    // Specular glint on Guide orb
    SolidBrush glintBrush(Color(200, 255, 255, 255));
    g.FillEllipse(&glintBrush, centerX - 11.0f, centerY - 11.0f, 8.0f, 8.0f);

    // Left Analog Stick (Top Left)
    float lxCenter = gpBody.X + gpBody.Width * 0.24f;
    float lyCenter = gpBody.Y + gpBody.Height * 0.38f;
    float stickR = 48.0f;

    SolidBrush stickWellBrush(Color(220, 6, 18, 38));
    g.FillEllipse(&stickWellBrush, lxCenter - stickR, lyCenter - stickR, stickR * 2.0f, stickR * 2.0f);
    Pen stickBorderPen(Color(255, 0, 229, 255), 2.0f);
    g.DrawEllipse(&stickBorderPen, lxCenter - stickR, lyCenter - stickR, stickR * 2.0f, stickR * 2.0f);

    float dotLx = lxCenter;
    float dotLy = lyCenter;
    if (has_input) {
        dotLx += (float)((pkt.thumb_lx / 32767.0) * (stickR - 16.0f));
        dotLy -= (float)((pkt.thumb_ly / 32767.0) * (stickR - 16.0f));
    }
    Color l3Col = (pkt.buttons & BTN_LEFT_THUMB) ? Color(255, 255, 23, 68) : Color(255, 0, 230, 118);
    SolidBrush l3Brush(l3Col);
    g.FillEllipse(&l3Brush, dotLx - 16.0f, dotLy - 16.0f, 32.0f, 32.0f);
    g.FillEllipse(&glintBrush, dotLx - 10.0f, dotLy - 10.0f, 7.0f, 7.0f);

    RectF lxTextRc(lxCenter - 60.0f, lyCenter + stickR + 6.0f, 120.0f, 16.0f);
    g.DrawString(L"Left Stick (L3)", -1, &fontNormal, lxTextRc, &sfCenter, &subBrush);

    // D-Pad Cross (Bottom Left)
    float dpadX = gpBody.X + gpBody.Width * 0.28f;
    float dpadY = gpBody.Y + gpBody.Height * 0.74f;
    float dSize = 16.0f;

    auto drawDpadSegment = [&](float dx, float dy, float dw, float dh, bool active) {
        RectF drc(dx, dy, dw, dh);
        SolidBrush db(active ? Color(255, 0, 245, 255) : Color(180, 14, 38, 70));
        g.FillRectangle(&db, drc);
        Pen dp(active ? Color(255, 255, 255) : Color(160, 0, 229, 255), 1.0f);
        g.DrawRectangle(&dp, drc);
    };

    drawDpadSegment(dpadX - dSize / 2, dpadY - dSize * 1.8f, dSize, dSize, (pkt.buttons & BTN_DPAD_UP) != 0);
    drawDpadSegment(dpadX - dSize / 2, dpadY + dSize * 0.8f, dSize, dSize, (pkt.buttons & BTN_DPAD_DOWN) != 0);
    drawDpadSegment(dpadX - dSize * 1.8f, dpadY - dSize / 2, dSize, dSize, (pkt.buttons & BTN_DPAD_LEFT) != 0);
    drawDpadSegment(dpadX + dSize * 0.8f, dpadY - dSize / 2, dSize, dSize, (pkt.buttons & BTN_DPAD_RIGHT) != 0);

    // Right Analog Stick (Bottom Right)
    float rxCenter = gpBody.X + gpBody.Width * 0.72f;
    float ryCenter = gpBody.Y + gpBody.Height * 0.74f;

    g.FillEllipse(&stickWellBrush, rxCenter - stickR, ryCenter - stickR, stickR * 2.0f, stickR * 2.0f);
    g.DrawEllipse(&stickBorderPen, rxCenter - stickR, ryCenter - stickR, stickR * 2.0f, stickR * 2.0f);

    float dotRx = rxCenter;
    float dotRy = ryCenter;
    if (has_input) {
        dotRx += (float)((pkt.thumb_rx / 32767.0) * (stickR - 16.0f));
        dotRy -= (float)((pkt.thumb_ry / 32767.0) * (stickR - 16.0f));
    }
    Color r3Col = (pkt.buttons & BTN_RIGHT_THUMB) ? Color(255, 255, 23, 68) : Color(255, 0, 229, 255);
    SolidBrush r3Brush(r3Col);
    g.FillEllipse(&r3Brush, dotRx - 16.0f, dotRy - 16.0f, 32.0f, 32.0f);
    g.FillEllipse(&glintBrush, dotRx - 10.0f, dotRy - 10.0f, 7.0f, 7.0f);

    RectF rxTextRc(rxCenter - 60.0f, ryCenter + stickR + 6.0f, 120.0f, 16.0f);
    g.DrawString(L"Right Stick (R3)", -1, &fontNormal, rxTextRc, &sfCenter, &subBrush);

    // 3D Gem Action Buttons A, B, X, Y (Top Right)
    float btnCenterX = gpBody.X + gpBody.Width * 0.76f;
    float btnCenterY = gpBody.Y + gpBody.Height * 0.38f;
    float btnDist = 34.0f;
    float btnRad = 15.0f;

    auto draw3DGem = [&](float bx, float by, const wchar_t* label, bool pressed, Color baseCol) {
        Color c = pressed ? Color(255, 255, 255, 255) : baseCol;
        SolidBrush bb(c);
        g.FillEllipse(&bb, bx - btnRad, by - btnRad, btnRad * 2.0f, btnRad * 2.0f);

        Pen bp(Color(255, 255, 255, 255), 1.5f);
        g.DrawEllipse(&bp, bx - btnRad, by - btnRad, btnRad * 2.0f, btnRad * 2.0f);

        // Specular 3D Glint Dot
        g.FillEllipse(&glintBrush, bx - btnRad * 0.6f, by - btnRad * 0.6f, btnRad * 0.5f, btnRad * 0.5f);

        SolidBrush textB(pressed ? Color(255, 0, 0, 0) : Color(255, 255, 255, 255));
        RectF labelRc(bx - btnRad, by - btnRad, btnRad * 2.0f, btnRad * 2.0f);
        g.DrawString(label, -1, &fontBold, labelRc, &sfCenter, &textB);
    };

    draw3DGem(btnCenterX, btnCenterY + btnDist, L"A", (pkt.buttons & BTN_A) != 0, Color(255, 0, 230, 118));
    draw3DGem(btnCenterX + btnDist, btnCenterY, L"B", (pkt.buttons & BTN_B) != 0, Color(255, 255, 23, 68));
    draw3DGem(btnCenterX - btnDist, btnCenterY, L"X", (pkt.buttons & BTN_X) != 0, Color(255, 0, 176, 255));
    draw3DGem(btnCenterX, btnCenterY - btnDist, L"Y", (pkt.buttons & BTN_Y) != 0, Color(255, 255, 214, 0));

    // LB / RB Bumpers at Top
    auto drawBumper = [&](float bx, float by, float bw, float bh, const wchar_t* label, bool pressed) {
        RectF brc(bx, by, bw, bh);
        GraphicsPath bp;
        AddRoundedRect(bp, brc, 8.0f);
        SolidBrush bg(pressed ? Color(255, 0, 229, 255) : Color(180, 18, 48, 88));
        g.FillPath(&bg, &bp);
        Pen bpen(Color(255, 0, 229, 255), 1.2f);
        g.DrawPath(&bpen, &bp);
        SolidBrush txtB(pressed ? Color(255, 0, 0, 0) : Color(255, 224, 247, 255));
        g.DrawString(label, -1, &fontBold, brc, &sfCenter, &txtB);
    };

    drawBumper(gpBody.X + 80.0f, gpBody.Y - 14.0f, 76.0f, 24.0f, L"LB", (pkt.buttons & BTN_LEFT_SHOULDER) != 0);
    drawBumper(gpBody.X + gpBody.Width - 156.0f, gpBody.Y - 14.0f, 76.0f, 24.0f, L"RB", (pkt.buttons & BTN_RIGHT_SHOULDER) != 0);

    // LT & RT Fluid Trigger Depth Meters
    RectF ltLabelRc(gpBody.X + 175.0f, gpBody.Y + 12.0f, 30.0f, 18.0f);
    RectF rtLabelRc(gpBody.X + gpBody.Width - 215.0f, gpBody.Y + 12.0f, 30.0f, 18.0f);
    g.DrawString(L"LT", -1, &fontBold, ltLabelRc, &sfCenter, &skyBrush);
    g.DrawString(L"RT", -1, &fontBold, rtLabelRc, &sfCenter, &skyBrush);

    RectF ltBar(gpBody.X + 210.0f, gpBody.Y + 14.0f, 74.0f, 14.0f);
    RectF rtBar(gpBody.X + gpBody.Width - 180.0f, gpBody.Y + 14.0f, 74.0f, 14.0f);

    SolidBrush barBg(Color(220, 6, 18, 38));
    Pen barPen(Color(180, 0, 229, 255), 1.0f);

    g.FillRectangle(&barBg, ltBar);
    g.DrawRectangle(&barPen, ltBar);
    g.FillRectangle(&barBg, rtBar);
    g.DrawRectangle(&barPen, rtBar);

    if (has_input) {
        float ltFill = (pkt.trigger_left * 74.0f) / 255.0f;
        if (ltFill > 1.0f) {
            RectF ltFillRc(ltBar.X, ltBar.Y, ltFill, ltBar.Height);
            SolidBrush ltFillB(Color(255, 0, 230, 118));
            g.FillRectangle(&ltFillB, ltFillRc);
        }
        float rtFill = (pkt.trigger_right * 74.0f) / 255.0f;
        if (rtFill > 1.0f) {
            RectF rtFillRc(rtBar.X, rtBar.Y, rtFill, rtBar.Height);
            SolidBrush rtFillB(Color(255, 0, 229, 255));
            g.FillRectangle(&rtFillB, rtFillRc);
        }
    }
}

} // namespace WifiGamepad

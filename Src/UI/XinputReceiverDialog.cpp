/**
 * XinputReceiverDialog.cpp
 *
 * Win32 ダイアログ実装 - XInput リモートコントローラー管理UI
 */

#ifdef SUPERMODEL_WIN32

#include "XinputReceiverDialog.h"
#include <commctrl.h>
#include <cstdio>
#include <string>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// グローバルインスタンス
XinputReceiverDialog* g_xinputDialog = nullptr;

// ウィンドウサイズ
static constexpr int DLG_W = 400;
static constexpr int DLG_H = 320;

// スロットごとの色
static COLORREF s_colorLocal   = RGB(40, 100, 40);
static COLORREF s_colorRemote  = RGB(40, 80, 160);
static COLORREF s_colorPhysical= RGB(60, 60, 60);
static COLORREF s_colorConnected = RGB(20, 130, 20);

// ---------------------------------------------------------------------------
// コンストラクタ / デストラクタ
// ---------------------------------------------------------------------------
XinputReceiverDialog::XinputReceiverDialog()
{
    g_xinputDialog = this;
}

XinputReceiverDialog::~XinputReceiverDialog()
{
    Stop();
    g_xinputDialog = nullptr;
}

// ---------------------------------------------------------------------------
// 非同期起動（別スレッドでウィンドウを作成）
// ---------------------------------------------------------------------------
bool XinputReceiverDialog::StartAsync()
{
    m_running.store(true);
    m_thread = std::thread(&XinputReceiverDialog::WindowThread, this);
    return true;
}

void XinputReceiverDialog::Stop()
{
    if (m_hwnd)
    {
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
        m_hwnd = nullptr;
    }
    if (m_thread.joinable())
        m_thread.join();
    m_slotManager.Shutdown();
}

// ---------------------------------------------------------------------------
// ウィンドウスレッド
// ---------------------------------------------------------------------------
void XinputReceiverDialog::WindowThread()
{
    // ウィンドウクラス登録
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30)); // ダークテーマ
    wc.lpszClassName = L"XinputReceiverDlg";
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    // ウィンドウ作成
    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_TOPMOST,
        L"XinputReceiverDlg",
        L"XInput Remote Controller Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, DLG_W, DLG_H,
        nullptr, nullptr, GetModuleHandle(nullptr), this);

    if (!m_hwnd)
    {
        m_running.store(false);
        return;
    }

    // コントロール作成
    CreateControls(m_hwnd);

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    // RemoteSlotManager 初期化
    auto slotCb = [this](int slot, const SlotState& state)
    {
        // UIスレッドに通知（PostMessage で安全に）
        if (m_hwnd)
            PostMessage(m_hwnd, WM_UPDATE_SLOT, (WPARAM)slot, 0);
    };
    auto statusCb = [this](const std::string& msg)
    {
        m_pendingStatus = msg;
        if (m_hwnd)
            PostMessage(m_hwnd, WM_UPDATE_STATUS, 0, 0);
    };

    m_slotManager.Initialize(slotCb, statusCb);

    // ホストIDと外部IP表示
    std::string hostIdText = "Host ID: " + m_slotManager.GetHostId();
    SetWindowTextA(m_lblHostId, hostIdText.c_str());

    // メッセージループ
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    m_running.store(false);
}

// ---------------------------------------------------------------------------
// コントロール作成
// ---------------------------------------------------------------------------
void XinputReceiverDialog::CreateControls(HWND hwnd)
{
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Segoe UI");

    // ─── ヘッダー ───────────────────────────────────────
    HWND hTitle = CreateWindowExW(0, L"STATIC", L"XInput Remote Receiver",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 8, DLG_W, 24, hwnd, nullptr, nullptr, nullptr);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowLongPtr(hTitle, GWL_ID, 200);

    // ─── スロット行（P1〜P4）───────────────────────────
    const wchar_t* slotNames[] = { L"", L"P1", L"P2", L"P3", L"P4" };
    for (int slot = 1; slot <= 4; slot++)
    {
        int y = 40 + (slot - 1) * 52;

        // スロット名ラベル
        HWND hName = CreateWindowExW(0, L"STATIC", slotNames[slot],
            WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
            10, y, 30, 36, hwnd, nullptr, nullptr, nullptr);
        SendMessage(hName, WM_SETFONT, (WPARAM)hFont, TRUE);

        // LOCAL/REMOTE ボタン
        m_btnSlot[slot] = CreateWindowExW(0, L"BUTTON", L"LOCAL",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            44, y + 4, 90, 28,
            hwnd, (HMENU)(UINT_PTR)(IDC_BTN_SLOT_BASE + slot),
            GetModuleHandle(nullptr), nullptr);
        SendMessage(m_btnSlot[slot], WM_SETFONT, (WPARAM)hFont, TRUE);

        // 状態ラベル
        m_lblSlot[slot] = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
            144, y + 4, 240, 28, hwnd,
            (HMENU)(UINT_PTR)(IDC_LBL_SLOT_BASE + slot),
            GetModuleHandle(nullptr), nullptr);
        SendMessage(m_lblSlot[slot], WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    // ─── 情報パネル ──────────────────────────────────────
    int infoY = 256;

    m_lblHostId = CreateWindowExW(0, L"STATIC", L"Host ID: -",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, infoY, 200, 18, hwnd,
        (HMENU)(UINT_PTR)IDC_LBL_HOSTID, nullptr, nullptr);
    SendMessage(m_lblHostId, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_lblIp = CreateWindowExW(0, L"STATIC", L"外部IP: 取得中...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        210, infoY, 180, 18, hwnd,
        (HMENU)(UINT_PTR)IDC_LBL_IP, nullptr, nullptr);
    SendMessage(m_lblIp, WM_SETFONT, (WPARAM)hFont, TRUE);

    // ステータスバー
    m_lblStatus = CreateWindowExW(0, L"STATIC", L"初期化中...",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN,
        0, DLG_H - 42, DLG_W, 20, hwnd,
        (HMENU)(UINT_PTR)IDC_LBL_STATUS, nullptr, nullptr);
    SendMessage(m_lblStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
}

// ---------------------------------------------------------------------------
// スロットUI更新
// ---------------------------------------------------------------------------
void XinputReceiverDialog::UpdateSlotUI(int slot)
{
    if (slot < 1 || slot > 4 || !m_btnSlot[slot]) return;

    const SlotState& state = m_slotManager.GetSlotState(slot);

    // ボタンテキスト
    if (state.isPhysical)
    {
        SetWindowTextA(m_btnSlot[slot], "LOCAL");
        EnableWindow(m_btnSlot[slot], FALSE);
    }
    else if (state.mode == SlotMode::REMOTE)
    {
        SetWindowTextA(m_btnSlot[slot], "REMOTE");
        EnableWindow(m_btnSlot[slot], TRUE);
    }
    else
    {
        SetWindowTextA(m_btnSlot[slot], "LOCAL");
        EnableWindow(m_btnSlot[slot], TRUE);
    }

    // 状態ラベル
    SetWindowTextA(m_lblSlot[slot], state.label.c_str());

    // 外部IP が取得できていたら更新
    const std::string& ip = m_slotManager.GetExternalIp();
    if (!ip.empty())
    {
        std::string ipText = "外部IP: " + ip;
        SetWindowTextA(m_lblIp, ipText.c_str());
    }
}

void XinputReceiverDialog::UpdateStatusBar(const std::string& msg)
{
    if (m_lblStatus)
        SetWindowTextA(m_lblStatus, msg.c_str());
}

void XinputReceiverDialog::OnSlotButtonClicked(int slot)
{
    m_slotManager.ToggleSlotMode(slot);
}

// ---------------------------------------------------------------------------
// Win32 ウィンドウプロシージャ
// ---------------------------------------------------------------------------
LRESULT CALLBACK XinputReceiverDialog::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    XinputReceiverDialog* self = nullptr;

    if (msg == WM_CREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<XinputReceiverDialog*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        if (self) self->m_hwnd = hwnd;
    }
    else
    {
        self = reinterpret_cast<XinputReceiverDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self)
        return self->HandleMessage(hwnd, msg, wp, lp);

    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT XinputReceiverDialog::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND:
    {
        int id = LOWORD(wp);
        if (id >= IDC_BTN_SLOT_BASE + 1 && id <= IDC_BTN_SLOT_BASE + 4)
            OnSlotButtonClicked(id - IDC_BTN_SLOT_BASE);
        return 0;
    }

    case WM_UPDATE_SLOT:
        UpdateSlotUI((int)wp);
        return 0;

    case WM_UPDATE_STATUS:
        UpdateStatusBar(m_pendingStatus);
        return 0;

    case WM_CTLCOLORSTATIC:
    {
        // ダークテーマ: 静的コントロールの背景・文字色
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, RGB(30, 30, 30));
        SetTextColor(hdc, RGB(220, 220, 220));
        return (LRESULT)CreateSolidBrush(RGB(30, 30, 30));
    }

    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, RGB(50, 50, 50));
        SetTextColor(hdc, RGB(220, 220, 220));
        return (LRESULT)CreateSolidBrush(RGB(50, 50, 50));
    }

    case WM_ERASEBKGND:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(RGB(30, 30, 30));
        FillRect((HDC)wp, &rc, br);
        DeleteObject(br);
        return 1;
    }

    case WM_CLOSE:
        // ウィンドウを閉じてもエミュレーターは継続
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

#endif // SUPERMODEL_WIN32

/**
 * XinputReceiverDialog.h
 *
 * Win32 ダイアログ - XInput リモートコントローラー管理UI
 * VB.NET の Form1 に相当する別ウィンドウ
 *
 * エミュレーターと同時に表示され、P1〜P4スロットの
 * LOCAL/REMOTE 切り替えを行う
 */
#pragma once

#ifdef SUPERMODEL_WIN32

#include <windows.h>
#include <string>
#include <thread>
#include "Remote/RemoteSlotManager.h"

class XinputReceiverDialog
{
public:
    XinputReceiverDialog();
    ~XinputReceiverDialog();

    // エミュレーターと同時起動（別スレッドでウィンドウを作成）
    bool StartAsync();
    // ウィンドウを閉じてスレッドを停止
    void Stop();

    bool IsRunning() const { return m_running; }

    // RemoteSlotManager へのアクセス（エミュレーター側から使用可能）
    RemoteSlotManager& GetSlotManager() { return m_slotManager; }

private:
    // ウィンドウスレッド本体
    void WindowThread();

    // Win32 コールバック
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    // UI 作成・更新
    void CreateControls(HWND hwnd);
    void UpdateSlotUI(int slot);
    void UpdateStatusBar(const std::string& msg);

    // スロットボタン が押されたとき
    void OnSlotButtonClicked(int slot);

    // ウィンドウ定数
    static constexpr int WM_UPDATE_SLOT   = WM_USER + 1;
    static constexpr int WM_UPDATE_STATUS = WM_USER + 2;

    // コントロールID
    static constexpr int IDC_BTN_SLOT_BASE  = 100; // 100〜103: スロット1〜4ボタン
    static constexpr int IDC_LBL_SLOT_BASE  = 110; // 110〜113: スロット1〜4ラベル
    static constexpr int IDC_LBL_HOSTID     = 120;
    static constexpr int IDC_LBL_IP         = 121;
    static constexpr int IDC_LBL_VIGEM      = 122;
    static constexpr int IDC_LBL_FIREBASE   = 123;
    static constexpr int IDC_LBL_STATUS     = 124;

    HWND m_hwnd = nullptr;
    HWND m_btnSlot[5]  = {};  // インデックス 1〜4
    HWND m_lblSlot[5]  = {};  // インデックス 1〜4
    HWND m_lblHostId   = nullptr;
    HWND m_lblIp       = nullptr;
    HWND m_lblViGEm    = nullptr;
    HWND m_lblFirebase = nullptr;
    HWND m_lblStatus   = nullptr;

    std::thread       m_thread;
    std::atomic<bool> m_running{ false };

    RemoteSlotManager m_slotManager;
    std::string       m_pendingStatus;
};

// グローバルインスタンス（Main.cpp からアクセス）
extern XinputReceiverDialog* g_xinputDialog;

#endif // SUPERMODEL_WIN32

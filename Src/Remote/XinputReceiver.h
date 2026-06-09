/**
 * XinputReceiver.h
 *
 * UDP経由でリモートのXInput状態を受信し、ViGEmコントローラーに転送する
 *
 * ポート割り当て（VB.NET側と一致）:
 *   スロット1: XInput=5000
 *   スロット2: XInput=5004
 *   スロット3: XInput=5008
 *   スロット4: XInput=5012
 */
#pragma once

#ifdef SUPERMODEL_WIN32

// winsock.h と winsock2.h の二重インクルード競合を防止
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_   // winsock.h の自動インクルードを抑制
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <atomic>
#include <functional>
#include <array>
#include <string>

// XInput 状態パケット（UDPで送受信する構造体、VB.NET側と一致させること）
#pragma pack(push, 1)
struct XInputPacket
{
    WORD  wButtons;       // ボタンビットフィールド (XINPUT_GAMEPAD_* 定数)
    BYTE  bLeftTrigger;   // 左トリガー 0〜255
    BYTE  bRightTrigger;  // 右トリガー 0〜255
    SHORT sThumbLX;       // 左スティック X軸 -32768〜32767
    SHORT sThumbLY;       // 左スティック Y軸
    SHORT sThumbRX;       // 右スティック X軸
    SHORT sThumbRY;       // 右スティック Y軸
};
#pragma pack(pop)

// 受信コールバック: slot=1〜4, packet=受信データ
using XInputCallback = std::function<void(int slot, const XInputPacket& packet)>;

class XinputReceiver
{
public:
    XinputReceiver();
    ~XinputReceiver();

    // Winsock初期化（プログラム全体で1回）
    static bool InitWinsock();
    static void CleanupWinsock();

    // 指定スロットのUDP受信を開始
    bool StartListening(int slot, int port, XInputCallback callback);
    // 指定スロットのUDP受信を停止
    void StopListening(int slot);
    // 全スロットを停止
    void StopAll();

    bool IsListening(int slot) const;
    std::string GetLastError() const { return m_lastError; }

    // スロット番号からポート番号を取得
    static int SlotToPort(int slot);

private:
    struct SlotState
    {
        std::thread       thread;
        std::atomic<bool> running{ false };
        SOCKET            sock = INVALID_SOCKET;
    };

    void ListenThread(int slot, int port, XInputCallback callback);

    std::array<SlotState, 5> m_slots; // インデックス 1〜4 を使用
    std::string              m_lastError;

    bool IsValidSlot(int slot) const { return slot >= 1 && slot <= 4; }
};

#endif // SUPERMODEL_WIN32

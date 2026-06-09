/**
 * XinputReceiver.cpp
 *
 * UDP経由でリモートXInput状態を受信する実装
 */

#ifdef SUPERMODEL_WIN32

#include "XinputReceiver.h"
#include <ws2tcpip.h>
#include <cstdio>

#pragma comment(lib, "ws2_32.lib")

// スロット番号 → XInput UDPポート番号
// VB.NET (Form1.vb) の SlotXInputPort と一致させること
static const int s_slotPorts[5] = {0, 5000, 5004, 5008, 5012};

// ---------------------------------------------------------------------------
// 静的メンバー: Winsock 初期化/クリーンアップ
// ---------------------------------------------------------------------------
bool XinputReceiver::InitWinsock()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "WSAStartup 失敗: %d", result);
        return false;
    }
    return true;
}

void XinputReceiver::CleanupWinsock()
{
    WSACleanup();
}

int XinputReceiver::SlotToPort(int slot)
{
    if (slot < 1 || slot > 4)
        return 0;
    return s_slotPorts[slot];
}

// ---------------------------------------------------------------------------
// コンストラクタ / デストラクタ
// ---------------------------------------------------------------------------
XinputReceiver::XinputReceiver()
{
}

XinputReceiver::~XinputReceiver()
{
    StopAll();
}

// ---------------------------------------------------------------------------
// 受信開始
// ---------------------------------------------------------------------------
bool XinputReceiver::StartListening(int slot, int port, XInputCallback callback)
{
    if (!IsValidSlot(slot))
        return false;

    // 既に動いていれば停止
    if (m_slots[slot].running.load())
        StopListening(slot);

    m_slots[slot].running.store(true);
    m_slots[slot].thread = std::thread(&XinputReceiver::ListenThread, this, slot, port, callback);
    return true;
}

// ---------------------------------------------------------------------------
// 受信停止
// ---------------------------------------------------------------------------
void XinputReceiver::StopListening(int slot)
{
    if (!IsValidSlot(slot))
        return;

    m_slots[slot].running.store(false);

    // ソケットをクローズしてスレッドを強制的に解除
    if (m_slots[slot].sock != INVALID_SOCKET)
    {
        closesocket(m_slots[slot].sock);
        m_slots[slot].sock = INVALID_SOCKET;
    }

    if (m_slots[slot].thread.joinable())
        m_slots[slot].thread.join();
}

void XinputReceiver::StopAll()
{
    for (int i = 1; i <= 4; i++)
        StopListening(i);
}

bool XinputReceiver::IsListening(int slot) const
{
    if (!IsValidSlot(slot))
        return false;
    return m_slots[slot].running.load();
}

// ---------------------------------------------------------------------------
// 受信スレッド本体
// ---------------------------------------------------------------------------
void XinputReceiver::ListenThread(int slot, int port, XInputCallback callback)
{
    // UDPソケット作成
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        m_lastError = "socket() 失敗";
        m_slots[slot].running.store(false);
        return;
    }

    m_slots[slot].sock = sock;

    // タイムアウト設定（500ms）でスレッド停止チェックを行う
    DWORD timeout = 500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));

    // バインド
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "bind() 失敗 port=%d err=%d", port, WSAGetLastError());
        m_lastError = buf;
        closesocket(sock);
        m_slots[slot].sock = INVALID_SOCKET;
        m_slots[slot].running.store(false);
        return;
    }

    printf("[XinputReceiver] slot%d UDP receive start  port=%d\n", slot, port);

    // 受信ループ
    while (m_slots[slot].running.load())
    {
        XInputPacket packet = {};
        sockaddr_in fromAddr = {};
        int fromLen = sizeof(fromAddr);



        // 受信バッファを大きめに確保
        char recvBuf[64] = {};
        int received = recvfrom(sock, recvBuf, sizeof(recvBuf), 0,
                                (sockaddr *)&fromAddr, &fromLen);

        if (received >= (int)sizeof(XInputPacket))
        {
            XInputPacket packet = {};
            memcpy(&packet, recvBuf, sizeof(XInputPacket));
            //printf("[XinputReceiver] packet received slot=%d bytes=%d\n", slot, received);
            if (callback)
                callback(slot, packet);
        }
        else if (received == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK)
                continue; // タイムアウトは正常（停止チェックのため）
            if (err == WSAENOTSOCK || err == WSAEINTR)
                break; // ソケットが閉じられた
        }
    }

    closesocket(sock);
    m_slots[slot].sock = INVALID_SOCKET;
    m_slots[slot].running.store(false);
    printf("[XinputReceiver] スロット%d UDP受信停止\n", slot);
}

#endif // SUPERMODEL_WIN32

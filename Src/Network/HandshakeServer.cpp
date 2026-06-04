#include "HandshakeServer.h"
#include <winsock2.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

#define TO_SOCKET(p) (reinterpret_cast<SOCKET>(p))

bool HandshakeServer::Start(int port, int width, int height,
                            OnConnectedCallback onConn,
                            OnDisconnectedCallback onDisconn,
                            OnPunchHoleCallback onPunchHole)
{
    m_onPunchHole = onPunchHole;
    m_port = port;
    m_onConnected = onConn;
    m_onDisconnected = onDisconn;

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        printf("[Handshake] socket() failed\n");
        return false;
    }

    // タイムアウト設定
    int timeout = 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               (char *)&timeout, sizeof(timeout));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        printf("[Handshake] bind() failed: %d\n", WSAGetLastError());
        closesocket(sock);
        return false;
    }

    m_socket = reinterpret_cast<void *>(sock);
    m_running.store(true);

    m_listenThread = CreateThread(nullptr, 0, ListenThreadProc, this, 0, nullptr);
    m_heartbeatThread = CreateThread(nullptr, 0, HeartbeatThreadProc, this, 0, nullptr);

    // PunchHole受信用スレッド
    m_punchThread4 = CreateThread(nullptr, 0, PunchHoleThreadProc4, this, 0, nullptr);
    m_punchThread5 = CreateThread(nullptr, 0, PunchHoleThreadProc5, this, 0, nullptr);
    printf("[Handshake] Listening on port %d\n", port);
    return true;
}

unsigned long __stdcall HandshakeServer::ListenThreadProc(void *param)
{
    static_cast<HandshakeServer *>(param)->ListenLoop();
    return 0;
}

unsigned long __stdcall HandshakeServer::HeartbeatThreadProc(void *param)
{
    static_cast<HandshakeServer *>(param)->HeartbeatLoop();
    return 0;
}

void HandshakeServer::ListenLoop()
{
    char buf[64];
    sockaddr_in client = {};
    int clientLen = sizeof(client);

    while (m_running.load())
    {
        int received = recvfrom(TO_SOCKET(m_socket), buf, sizeof(buf) - 1, 0,
                                (sockaddr *)&client, &clientLen);
        if (received <= 0)
            continue;

        buf[received] = '\0';

        if (strcmp(buf, "HELLO") == 0)
        {
            char clientIP[32];
            strncpy(clientIP, inet_ntoa(client.sin_addr), sizeof(clientIP) - 1);
            printf("[Handshake] HELLO from %s\n", clientIP);

            // OKを返す
            char ok[32];
            snprintf(ok, sizeof(ok), "OK %d %d", m_width, m_height);
            sendto(TO_SOCKET(m_socket), ok, (int)strlen(ok), 0,
                   (sockaddr *)&client, clientLen);

            if (!m_connected.load())
            {
                m_connected.store(true);
                m_lastHeartbeat.store(GetTickCount());
                if (m_onConnected)
                    m_onConnected(clientIP);
            }
        }
        else if (strcmp(buf, "HB") == 0)
        {
            // ハートビート
            printf("[Handshake] HB received\n"); 
            m_lastHeartbeat.store(GetTickCount());
        }
    }
}

void HandshakeServer::HeartbeatLoop()
{
    while (m_running.load())
    {
        Sleep(1000);
        if (m_connected.load())
        {
            uint32_t now = GetTickCount();
            uint32_t last = m_lastHeartbeat.load();
            if (now - last > 5000)
            {
                printf("[Handshake] Client disconnected (timeout)\n");
                m_connected.store(false);
                if (m_onDisconnected)
                    m_onDisconnected();
            }
        }
    }
}

void HandshakeServer::NotifyHeartbeat()
{
    m_lastHeartbeat.store(GetTickCount());
}

void HandshakeServer::Stop()
{
    m_running.store(false);
    if (m_socket)
    {
        closesocket(TO_SOCKET(m_socket));
        m_socket = nullptr;
    }
    if (m_listenThread)
    {
        WaitForSingleObject(m_listenThread, 3000);
        CloseHandle(m_listenThread);
        m_listenThread = nullptr;
    }
    if (m_heartbeatThread)
    {
        WaitForSingleObject(m_heartbeatThread, 3000);
        CloseHandle(m_heartbeatThread);
        m_heartbeatThread = nullptr;
    }
    if (m_punchThread4)
    {
        WaitForSingleObject(m_punchThread4, 3000);
        CloseHandle(m_punchThread4);
        m_punchThread4 = nullptr;
    }
    if (m_punchThread5)
    {
        WaitForSingleObject(m_punchThread5, 3000);
        CloseHandle(m_punchThread5);
        m_punchThread5 = nullptr;
    }
    printf("[Handshake] Stopped\n");
}

void HandshakeServer::PunchHoleLoop(int port)
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (sockaddr *)&addr, sizeof(addr));

    char buf[16];
    sockaddr_in client = {};
    int clientLen = sizeof(client);

    while (m_running.load())
    {
        int recv = recvfrom(sock, buf, sizeof(buf), 0,
                            (sockaddr *)&client, &clientLen);
        if (recv <= 0)
            continue;

        int clientPort = ntohs(client.sin_port);
        char clientIP[32];
        strncpy(clientIP, inet_ntoa(client.sin_addr), sizeof(clientIP) - 1);

        printf("[PunchHole] port %d from %s:%d\n", port, clientIP, clientPort);

        if (m_onPunchHole)
            m_onPunchHole(clientIP, port == 5004 ? clientPort : 0,
                          port == 5005 ? clientPort : 0);
    }
    closesocket(sock);
}
unsigned long __stdcall HandshakeServer::PunchHoleThreadProc4(void *param)
{
    static_cast<HandshakeServer *>(param)->PunchHoleLoop(5004);
    return 0;
}

unsigned long __stdcall HandshakeServer::PunchHoleThreadProc5(void *param)
{
    static_cast<HandshakeServer *>(param)->PunchHoleLoop(5005);
    return 0;
}
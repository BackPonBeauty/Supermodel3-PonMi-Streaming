#pragma once
#include <string>
#include <functional>
#include <atomic>

class HandshakeServer
{
public:
    using OnConnectedCallback = std::function<void(const std::string &clientIP)>;
    using OnDisconnectedCallback = std::function<void()>;
    using OnPunchHoleCallback = std::function<void(const std::string &ip, int videoPort, int audioPort)>;
    

    HandshakeServer() = default;
    ~HandshakeServer() { Stop(); }

    bool Start(int port, int width, int height,
               OnConnectedCallback onConn,
               OnDisconnectedCallback onDisconn,
               OnPunchHoleCallback onPunchHole);
    void Stop();

    bool IsConnected() const { return m_connected.load(); }
    void NotifyHeartbeat(); // 受信スレッドから呼ぶ

private:
    OnPunchHoleCallback m_onPunchHole;
    void *m_punchThread4 = nullptr;
    void *m_punchThread5 = nullptr;
    static unsigned long __stdcall PunchHoleThreadProc4(void *param);
    static unsigned long __stdcall PunchHoleThreadProc5(void *param);
    void PunchHoleLoop(int port);
    int m_width = 960;
    int m_height = 540;
    static unsigned long __stdcall ListenThreadProc(void *param);
    static unsigned long __stdcall HeartbeatThreadProc(void *param);
    void ListenLoop();
    void HeartbeatLoop();

    void *m_socket = nullptr;
    void *m_listenThread = nullptr;
    void *m_heartbeatThread = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::atomic<uint32_t> m_lastHeartbeat{0};

    OnConnectedCallback m_onConnected;
    OnDisconnectedCallback m_onDisconnected;

    int m_port = 5006;
};
#pragma once
#include <cstdint>
#include <string>
#include <winsock2.h>

class RtpSender
{
public:
    RtpSender() = default;
    ~RtpSender() { Shutdown(); }

    bool Init(const char *destIP, int destPort);
    void Send(const uint8_t *nalData, int size);
    void Shutdown();
    void SetDestPort(int port)
    {
        m_dest.sin_port = htons((u_short)port);
        printf("[RTP] Destination Port changed to %d\n", port);
    }
    void SetDestIP(const std::string &ip)
    {
        m_dest.sin_addr.s_addr = inet_addr(ip.c_str());
        printf("[RTP] Destination IP changed to %s\n", ip.c_str());
    }

private:
    void SendRtpPacket(const uint8_t *data, int size, bool marker);

    SOCKET m_socket = INVALID_SOCKET;
    sockaddr_in m_dest = {};
    uint16_t m_seqNum = 0;
    uint32_t m_timestamp = 0;
    uint32_t m_ssrc = 0;

    static constexpr int RTP_MTU = 1400;
};
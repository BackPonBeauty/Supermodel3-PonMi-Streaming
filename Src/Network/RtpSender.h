#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <winsock2.h>

class RtpSender
{
public:
    RtpSender() = default;
    ~RtpSender() { Shutdown(); }

    bool Init(const char *destIP, int destPort);
    void Send(const uint8_t *nalData, int size);
    void Shutdown();
    void SetDestPort(int port);
    void SetDestIP(const std::string &ip);
    void SetDestIPs(const std::vector<std::string> &ips);

private:
    void SendRtpPacket(const uint8_t *data, int size, bool marker);

    SOCKET m_socket = INVALID_SOCKET;
    std::vector<sockaddr_in> m_dests;
    std::mutex m_destsMutex;
    int m_destPort = 0;

    uint16_t m_seqNum = 0;
    uint32_t m_timestamp = 0;
    uint32_t m_ssrc = 0;

    static constexpr int RTP_MTU = 1400;

};
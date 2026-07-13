#include "UdpAudioSender.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <mswsock.h>

bool UdpAudioSender::Init(const char *destIP, int destPort)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
    {
        printf("[AudioUDP] socket() failed: %d\n", WSAGetLastError());
        return false;
    }
    DWORD dwBytesReturned = 0;
    BOOL bNewBehavior = FALSE;
    WSAIoctl(m_socket, SIO_UDP_CONNRESET,
             &bNewBehavior, sizeof(bNewBehavior),
             NULL, 0, &dwBytesReturned, NULL, NULL);

    // バインド: 固定ポートで待ち受け
    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons((u_short)destPort);
    bindAddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(m_socket, (sockaddr *)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
    {
        printf("[AudioUDP] bind() failed: %d (port=%d)\n", WSAGetLastError(), destPort);
    }

    // 受信タイムアウト設定
    int timeout = 500;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    m_destPort = destPort;
    // 注意: HELLO受信による動的ポート学習まで m_dests は空のまま（自己ループ防止）

    // Opusエンコーダー初期化
    int err;
    m_encoder = opus_encoder_create(OPUS_SAMPLE_RATE, OPUS_CHANNELS,
                                    OPUS_APPLICATION_AUDIO, &err);
    if (err != OPUS_OK || !m_encoder)
    {
        printf("[AudioUDP] opus_encoder_create failed: %d\n", err);
        return false;
    }

    // 低遅延設定
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(128000)); // 128kbps
    opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(5));   // 中程度
    opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

    m_opusBuf.resize(OPUS_MAX_PACKET);
    m_resampleBuf.resize(OPUS_FRAME_SIZE * OPUS_CHANNELS);
    m_floatBuf.resize(OPUS_FRAME_SIZE * OPUS_CHANNELS);
    m_inputAccum.clear();

    printf("[AudioUDP] Ready (listening on port %d, no dest until HELLO)\n", destPort);

    // HELLOパケット受信スレッド起動
    m_helloRunning.store(true);
    m_helloThread = CreateThread(nullptr, 0, HelloRecvThreadProc, this, 0, nullptr);

    return true;
}

unsigned long __stdcall UdpAudioSender::HelloRecvThreadProc(void *param)
{
    static_cast<UdpAudioSender *>(param)->HelloRecvLoop();
    return 0;
}

void UdpAudioSender::HelloRecvLoop()
{
    char buf[64];
    sockaddr_in from = {};
    int fromLen = sizeof(from);

    while (m_helloRunning.load())
    {
        int received = recvfrom(m_socket, buf, sizeof(buf) - 1, 0,
                                (sockaddr *)&from, &fromLen);
        if (received <= 0)
            continue;

        buf[received] = '\0';

        if (strncmp(buf, "HELLO", 5) == 0)
        {
            std::string fromIP = inet_ntoa(from.sin_addr);
            int fromPort = ntohs(from.sin_port);
            std::string nick = "unknown";

            if (buf[5] == ':' && buf[6] != '\0')
                nick = std::string(buf + 6);

            printf("[AudioUDP] HELLO from %s:%d (nick=%s) -> learning dest\n",
                   fromIP.c_str(), fromPort, nick.c_str());

            {
                std::lock_guard<std::mutex> lock(m_destsMutex);

                bool found = false;
                for (auto &dest : m_dests)
                {
                    if (dest.nick == nick)
                    {
                        dest.addr.sin_addr.s_addr = inet_addr(fromIP.c_str());
                        dest.addr.sin_port = htons((u_short)fromPort);
                        found = true;
                        printf("[AudioUDP] Updated dest for nick=%s to %s:%d\n",
                               nick.c_str(), fromIP.c_str(), fromPort);
                        break;
                    }
                }

                if (!found)
                {
                    DestEntry entry;
                    entry.nick = nick;
                    entry.addr.sin_family = AF_INET;
                    entry.addr.sin_addr.s_addr = inet_addr(fromIP.c_str());
                    entry.addr.sin_port = htons((u_short)fromPort);
                    m_dests.push_back(entry);
                    printf("[AudioUDP] New dest added: nick=%s %s:%d (total=%zu)\n",
                           nick.c_str(), fromIP.c_str(), fromPort, m_dests.size());
                }
            }
        }
    }
}

void UdpAudioSender::SendWithTimestamp(const int16_t *pcm, int samples, int ch)
{
    if (m_socket == INVALID_SOCKET || !m_encoder)
        return;

    for (int i = 0; i < samples * ch; i++)
        m_inputAccum.push_back(pcm[i]);

    const int srcFrameSize = 882;
    const int frameBytes = srcFrameSize * OPUS_CHANNELS;

    // 次フレームの先頭1サンプルも見えるように +1 余裕を持って判定
    while ((int)m_inputAccum.size() >= frameBytes + OPUS_CHANNELS)
    {
        for (int i = 0; i < OPUS_FRAME_SIZE; i++)
        {
            float srcPos = (float)i * srcFrameSize / OPUS_FRAME_SIZE;
            int s0 = (int)srcPos;
            int s1 = s0 + 1;
            float f = srcPos - s0;

            m_resampleBuf[i * 2 + 0] = (int16_t)(m_inputAccum[s0 * 2 + 0] * (1.0f - f) +
                                                  m_inputAccum[s1 * 2 + 0] * f);
            m_resampleBuf[i * 2 + 1] = (int16_t)(m_inputAccum[s0 * 2 + 1] * (1.0f - f) +
                                                  m_inputAccum[s1 * 2 + 1] * f);
        }

        m_inputAccum.erase(m_inputAccum.begin(),
                           m_inputAccum.begin() + frameBytes);

        int encoded = opus_encode(m_encoder,
                                  m_resampleBuf.data(),
                                  OPUS_FRAME_SIZE,
                                  m_opusBuf.data(),
                                  OPUS_MAX_PACKET);
        if (encoded < 0)
            continue;

        std::vector<uint8_t> pkt(4 + encoded);
        pkt[0] = (m_timestamp >> 24) & 0xFF;
        pkt[1] = (m_timestamp >> 16) & 0xFF;
        pkt[2] = (m_timestamp >> 8)  & 0xFF;
        pkt[3] =  m_timestamp        & 0xFF;
        memcpy(pkt.data() + 4, m_opusBuf.data(), encoded);

        std::lock_guard<std::mutex> lock(m_destsMutex);
        for (const auto &dest : m_dests)
        {
            if (dest.addr.sin_port == 0) continue; // ポート未学習はスキップ
            sendto(m_socket, (const char *)pkt.data(), (int)pkt.size(), 0,
                   (sockaddr *)&dest.addr, sizeof(dest.addr));
        }

        m_timestamp += OPUS_FRAME_SIZE;
    }
}

void UdpAudioSender::SetDestIP(const std::string &ip)
{
    std::vector<std::string> ips = { ip };
    SetDestIPs(ips);
}

void UdpAudioSender::SetDestIPs(const std::vector<std::string> &ips)
{
    std::lock_guard<std::mutex> lock(m_destsMutex);
    m_dests.clear();
    for (const auto &ip : ips)
    {
        DestEntry entry;
        entry.nick = ip;
        entry.addr.sin_family = AF_INET;
        entry.addr.sin_port = htons((u_short)m_destPort);
        entry.addr.sin_addr.s_addr = inet_addr(ip.c_str());
        m_dests.push_back(entry);
    }
    printf("[AudioUDP] Destinations updated (%zu clients)\n", m_dests.size());
}

// SetDestEndpoints: "IP:nick" 形式のリストから宛先を更新
void UdpAudioSender::SetDestEndpoints(const std::vector<std::string> &ipNickList)
{
    std::lock_guard<std::mutex> lock(m_destsMutex);

    // 学習済みポートをニックネームで保存
    std::map<std::string, u_short> learnedPorts;
    std::map<std::string, u_long> learnedAddrs;
    for (const auto &dest : m_dests)
    {
        if (dest.addr.sin_port != 0)
        {
            learnedPorts[dest.nick] = dest.addr.sin_port;
            learnedAddrs[dest.nick] = dest.addr.sin_addr.s_addr;
        }
    }

    m_dests.clear();
    for (const auto &entry : ipNickList)
    {
        auto colonPos = entry.find(':');
        std::string ip   = (colonPos != std::string::npos) ? entry.substr(0, colonPos) : entry;
        std::string nick = (colonPos != std::string::npos) ? entry.substr(colonPos + 1) : entry;

        DestEntry de;
        de.nick = nick;
        de.addr.sin_family = AF_INET;

        if (learnedPorts.count(nick))
        {
            de.addr.sin_addr.s_addr = learnedAddrs[nick];
            de.addr.sin_port = learnedPorts[nick];
            printf("[AudioUDP] Restored learned port for nick=%s %s:%d\n",
                   nick.c_str(), inet_ntoa(de.addr.sin_addr), ntohs(de.addr.sin_port));
        }
        else
        {
            de.addr.sin_addr.s_addr = inet_addr(ip.c_str());
            de.addr.sin_port = 0; // 未学習: HELLOが届くまで送信しない
            printf("[AudioUDP] Awaiting HELLO for nick=%s (ip hint=%s)\n", nick.c_str(), ip.c_str());
        }
        m_dests.push_back(de);
    }
    printf("[AudioUDP] Destinations updated (%zu entries)\n", m_dests.size());
}

void UdpAudioSender::SetDestPort(int port)
{
    std::lock_guard<std::mutex> lock(m_destsMutex);
    m_destPort = port;
    for (auto &dest : m_dests)
    {
        if (dest.addr.sin_port != 0)
            dest.addr.sin_port = htons((u_short)port);
    }
    printf("[AudioUDP] Port changed to %d\n", port);
}

void UdpAudioSender::Shutdown()
{
    m_helloRunning.store(false);
    if (m_encoder)
    {
        opus_encoder_destroy(m_encoder);
        m_encoder = nullptr;
    }
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        printf("[AudioUDP] Shutdown\n");
    }
    if (m_helloThread)
    {
        WaitForSingleObject(m_helloThread, 2000);
        CloseHandle(m_helloThread);
        m_helloThread = nullptr;
    }
    std::lock_guard<std::mutex> lock(m_destsMutex);
    m_dests.clear();
}

void UdpAudioSender::ClearDests()
{
    std::lock_guard<std::mutex> lock(m_destsMutex);
    m_dests.clear();
    printf("[AudioUDP] Destinations cleared\n");
}

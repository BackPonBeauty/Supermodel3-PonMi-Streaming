#include "RtpSender.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <mswsock.h>

bool RtpSender::Init(const char *destIP, int destPort, bool useH265)
{
    m_useH265 = useH265;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
    {
        printf("[RTP] socket() failed: %d\n", WSAGetLastError());
        return false;
    }
    // ICMPエラーを無視（Port unreachable対策）
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
        printf("[RTP] bind() failed: %d (port=%d)\n", WSAGetLastError(), destPort);
        // バインド失敗は致命的ではない（送信専用ソケットとして動作）
    }

    // 受信タイムアウト設定
    int timeout = 500;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    m_destPort = destPort;
    // 注意: HELLO受信による動的ポート学習まで m_dests は空のまま（自己ループ防止）

    srand((unsigned)time(nullptr));
    m_ssrc = rand();
    m_seqNum = (uint16_t)rand();
    m_timestamp = rand();

    printf("[RTP] Ready (listening on port %d, no dest until HELLO)\n", destPort);

    // HELLOパケット受信スレッド起動
    m_helloRunning.store(true);
    m_helloThread = CreateThread(nullptr, 0, HelloRecvThreadProc, this, 0, nullptr);

    return true;
}

unsigned long __stdcall RtpSender::HelloRecvThreadProc(void *param)
{
    static_cast<RtpSender *>(param)->HelloRecvLoop();
    return 0;
}

void RtpSender::HelloRecvLoop()
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

            // ニックネームの抽出: "HELLO:nick_slotid" or "HELLO:nick"
            if (buf[5] == ':' && buf[6] != '\0')
            {
                std::string payload = std::string(buf + 6);
                // スロットIDのアンダースコアより前がニックネーム
                auto upos = payload.rfind('_');
                nick = (upos != std::string::npos) ? payload.substr(0, upos) : payload;
            }

            printf("[RTP] HELLO from %s:%d (nick=%s) -> learning dest\n",
                   fromIP.c_str(), fromPort, nick.c_str());

            {
                std::lock_guard<std::mutex> lock(m_destsMutex);

                // 同じニックネームのエントリを検索して更新（IPミスマッチに対応）
                bool found = false;
                for (auto &dest : m_dests)
                {
                    if (dest.nick == nick)
                    {
                        dest.addr.sin_addr.s_addr = inet_addr(fromIP.c_str());
                        dest.addr.sin_port = htons((u_short)fromPort);
                        found = true;
                        printf("[RTP] Updated dest for nick=%s to %s:%d\n",
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
                    printf("[RTP] New dest added: nick=%s %s:%d (total=%zu)\n",
                           nick.c_str(), fromIP.c_str(), fromPort, m_dests.size());
                }
            }
        }
    }
}

void RtpSender::Send(const uint8_t *nalData, int size)
{
    if (m_socket == INVALID_SOCKET || size <= 0)
        return;

    const uint8_t *nal = nalData;
    int nalSize = size;

    // スタートコード除去
    if (nalSize >= 4 && nal[0] == 0 && nal[1] == 0 && nal[2] == 0 && nal[3] == 1)
    {
        nal += 4;
        nalSize -= 4;
    }
    else if (nalSize >= 3 && nal[0] == 0 && nal[1] == 0 && nal[2] == 1)
    {
        nal += 3;
        nalSize -= 3;
    }

    if (nalSize <= 0)
        return;

    if (nalSize <= RTP_MTU)
    {
        SendRtpPacket(nal, nalSize, true);
    }
    else if (m_useH265)
    {
        // HEVC FU fragmentation
        uint8_t type = (nal[0] & 0x7E) >> 1;
        uint8_t payloadHeader0 = (49 << 1) | (nal[0] & 0x01);
        uint8_t payloadHeader1 = nal[1];
        const uint8_t *payload = nal + 2;
        int remaining = nalSize - 2;
        bool first = true;

        while (remaining > 0)
        {
            int fragSize = (remaining > RTP_MTU - 3) ? RTP_MTU - 3 : remaining;
            bool last = (remaining <= RTP_MTU - 3);

            uint8_t fuBuf[RTP_MTU + 3];
            fuBuf[0] = payloadHeader0;
            fuBuf[1] = payloadHeader1;
            fuBuf[2] = (first ? 0x80 : 0) | (last ? 0x40 : 0) | type;
            memcpy(fuBuf + 3, payload, fragSize);

            SendRtpPacket(fuBuf, fragSize + 3, last);

            payload += fragSize;
            remaining -= fragSize;
            first = false;
        }
    }
    else
    {
        // H.264 FU-A fragmentation
        uint8_t type = nal[0] & 0x1F;
        uint8_t nri = nal[0] & 0x60;
        uint8_t fuIndicator = nri | 28; // FU-A type is 28
        const uint8_t *payload = nal + 1;
        int remaining = nalSize - 1;
        bool first = true;

        while (remaining > 0)
        {
            int fragSize = (remaining > RTP_MTU - 2) ? RTP_MTU - 2 : remaining;
            bool last = (remaining <= RTP_MTU - 2);

            uint8_t fuBuf[RTP_MTU + 2];
            fuBuf[0] = fuIndicator;
            fuBuf[1] = (first ? 0x80 : 0) | (last ? 0x40 : 0) | type;
            memcpy(fuBuf + 2, payload, fragSize);

            SendRtpPacket(fuBuf, fragSize + 2, last);

            payload += fragSize;
            remaining -= fragSize;
            first = false;
        }
    }

    m_timestamp += 1500; // 90kHz基準 60fps
}

void RtpSender::SendRtpPacket(const uint8_t *data, int size, bool marker)
{
    uint8_t buf[2048];

    buf[0] = 0x80;
    buf[1] = (marker ? 0x80 : 0) | 96;
    buf[2] = (m_seqNum >> 8) & 0xFF;
    buf[3] = m_seqNum & 0xFF;
    buf[4] = (m_timestamp >> 24) & 0xFF;
    buf[5] = (m_timestamp >> 16) & 0xFF;
    buf[6] = (m_timestamp >> 8) & 0xFF;
    buf[7] = m_timestamp & 0xFF;
    buf[8] = (m_ssrc >> 24) & 0xFF;
    buf[9] = (m_ssrc >> 16) & 0xFF;
    buf[10] = (m_ssrc >> 8) & 0xFF;
    buf[11] = m_ssrc & 0xFF;

    memcpy(buf + 12, data, size);

    std::lock_guard<std::mutex> lock(m_destsMutex);
    int sent = 0;
    for (const auto &dest : m_dests)
    {
        if (dest.addr.sin_port == 0) continue; // ポート未学習はスキップ
        sendto(m_socket, (char *)buf, size + 12, 0,
               (sockaddr *)&dest.addr, sizeof(dest.addr));
        sent = size + 12;
    }
    if (sent > 0)
        m_bytesSentAcc.fetch_add((uint64_t)sent, std::memory_order_relaxed);
    m_seqNum++;
}

float RtpSender::GetBitrateBps()
{
    uint32_t now = GetTickCount();
    uint32_t last = m_lastBitrateTime.load(std::memory_order_relaxed);
    uint32_t elapsed = now - last;
    if (elapsed >= 500 && last != 0)
    {
        uint64_t bytes = m_bytesSentAcc.exchange(0, std::memory_order_relaxed);
        float bps = (float)bytes * 8.0f * 1000.0f / (float)elapsed;
        m_bitrateBps.store(bps, std::memory_order_relaxed);
        m_lastBitrateTime.store(now, std::memory_order_relaxed);
    }
    else if (last == 0)
    {
        m_lastBitrateTime.store(now, std::memory_order_relaxed);
    }
    return m_bitrateBps.load(std::memory_order_relaxed);
}

void RtpSender::Shutdown()
{
    m_helloRunning.store(false);
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        WSACleanup();
        printf("[RTP] Shutdown\n");
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

void RtpSender::SetDestPort(int port)
{
    std::lock_guard<std::mutex> lock(m_destsMutex);
    m_destPort = port;
    for (auto &dest : m_dests)
    {
        if (dest.addr.sin_port != 0)
            dest.addr.sin_port = htons((u_short)port);
    }
    printf("[RTP] Destination Port changed to %d\n", port);
}

void RtpSender::SetDestIP(const std::string &ip)
{
    std::vector<std::string> ips = { ip };
    SetDestIPs(ips);
}

// SetDestIPs: 単純なIPリスト（LinkPlay=1などから呼ばれる）
void RtpSender::SetDestIPs(const std::vector<std::string> &ips)
{
    std::lock_guard<std::mutex> lock(m_destsMutex);
    m_dests.clear();
    for (const auto &ip : ips)
    {
        DestEntry entry;
        entry.nick = ip; // ニックネーム代わりにIPを使用
        entry.addr.sin_family = AF_INET;
        entry.addr.sin_port = htons((u_short)m_destPort);
        entry.addr.sin_addr.s_addr = inet_addr(ip.c_str());
        m_dests.push_back(entry);
    }
    printf("[RTP] Destinations updated (%zu clients)\n", m_dests.size());
}

// SetDestEndpoints: "IP:nick" 形式のリストから宛先を更新
// すでに学習済みのポートを nick をキーに引き継ぐ
void RtpSender::SetDestEndpoints(const std::vector<std::string> &ipNickList)
{
    std::lock_guard<std::mutex> lock(m_destsMutex);

    // 現在の学習済みポートをニックネームで保存
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
        // "IP:nick" を分解
        auto colonPos = entry.find(':');
        std::string ip   = (colonPos != std::string::npos) ? entry.substr(0, colonPos) : entry;
        std::string nick = (colonPos != std::string::npos) ? entry.substr(colonPos + 1) : entry;

        DestEntry de;
        de.nick = nick;
        de.addr.sin_family = AF_INET;

        // 学習済みポートがあれば引き継ぐ
        if (learnedPorts.count(nick))
        {
            de.addr.sin_addr.s_addr = learnedAddrs[nick]; // 学習済みIPを使用（127.0.0.1等）
            de.addr.sin_port = learnedPorts[nick];
            printf("[RTP] Restored learned port for nick=%s %s:%d\n",
                   nick.c_str(), inet_ntoa(de.addr.sin_addr), ntohs(de.addr.sin_port));
        }
        else
        {
            de.addr.sin_addr.s_addr = inet_addr(ip.c_str());
            de.addr.sin_port = 0; // 未学習: HELLOが届くまで送信しない
            printf("[RTP] Awaiting HELLO for nick=%s (ip hint=%s)\n", nick.c_str(), ip.c_str());
        }
        m_dests.push_back(de);
    }
    printf("[RTP] Destinations updated (%zu entries)\n", m_dests.size());
}

void RtpSender::ClearDests()
{
    std::lock_guard<std::mutex> lock(m_destsMutex);
    m_dests.clear();
    printf("[RTP] Destinations cleared\n");
}
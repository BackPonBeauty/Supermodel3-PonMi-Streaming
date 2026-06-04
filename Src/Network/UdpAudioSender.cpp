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

    memset(&m_dest, 0, sizeof(m_dest));
    m_dest.sin_family = AF_INET;
    m_dest.sin_port = htons((u_short)destPort);
    m_dest.sin_addr.s_addr = inet_addr(destIP);

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

    printf("[AudioUDP] Ready -> %s:%d (Opus 128kbps)\n", destIP, destPort);
    return true;
}

void UdpAudioSender::SendWithTimestamp(const int16_t *pcm, int samples, int ch)
{
    if (m_socket == INVALID_SOCKET || !m_encoder)
        return;

    for (int i = 0; i < samples * ch; i++)
        m_inputAccum.push_back(pcm[i]);

    // 44100→48000変換比率
    // 960フレーム @ 48000Hz = 20ms
    // 20ms @ 44100Hz = 882サンプル
    const int srcFrameSize = 882; // 44100Hzでの20ms分
    const int frameBytes = srcFrameSize * OPUS_CHANNELS;

    while ((int)m_inputAccum.size() >= frameBytes)
    {
        // 882サンプル→960サンプルに線形補間
        for (int i = 0; i < OPUS_FRAME_SIZE; i++)
        {
            float srcPos = (float)i * srcFrameSize / OPUS_FRAME_SIZE;
            int s0 = (int)srcPos;
            int s1 = s0 + 1;
            float f = srcPos - s0;

            if (s1 >= srcFrameSize)
                s1 = srcFrameSize - 1;

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
        pkt[2] = (m_timestamp >> 8) & 0xFF;
        pkt[3] = m_timestamp & 0xFF;
        memcpy(pkt.data() + 4, m_opusBuf.data(), encoded);

        sendto(m_socket, (const char *)pkt.data(), (int)pkt.size(), 0,
               (sockaddr *)&m_dest, sizeof(m_dest));

        m_timestamp += OPUS_FRAME_SIZE;
    }
}

void UdpAudioSender::SetDestIP(const std::string &ip)
{
    // エンコーダーは維持したままアドレスだけ変更
    m_dest.sin_addr.s_addr = inet_addr(ip.c_str());
    printf("[AudioUDP] ip changed to %s\n", ip.c_str());
}

void UdpAudioSender::Shutdown()
{
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
}

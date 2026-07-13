#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <winsock2.h>
#include <windows.h>
#include <opus/opus.h>
#include <utility>

class UdpAudioSender
{
public:
    UdpAudioSender() = default;
    ~UdpAudioSender() { Shutdown(); }

    bool Init(const char *destIP, int destPort);
    void SendWithTimestamp(const int16_t *pcm, int samples, int ch);
    void SetDestIP(const std::string &ip);
    void SetDestIPs(const std::vector<std::string> &ips);
    // "IP:nick" 形式のリストから宛先更新（学習済みポートをnickで引き継ぐ）
    void SetDestEndpoints(const std::vector<std::string> &ipNickList);
    void ClearDests();
    void Shutdown();
    void SetDestPort(int port);

private:
    void HelloRecvLoop();
    static unsigned long __stdcall HelloRecvThreadProc(void *param);

    struct DestEntry
    {
        std::string nick;
        sockaddr_in addr = {};
    };

    SOCKET m_socket = INVALID_SOCKET;
    std::vector<DestEntry> m_dests;
    std::mutex m_destsMutex;
    int m_destPort = 0;
    uint32_t m_timestamp = 0;

    HANDLE m_helloThread = nullptr;
    std::atomic<bool> m_helloRunning{false};

    // Opusエンコーダー
    OpusEncoder *m_encoder = nullptr;
    static constexpr int OPUS_SAMPLE_RATE = 48000;
    static constexpr int OPUS_CHANNELS = 2;
    static constexpr int OPUS_FRAME_SIZE = 960; // 20ms @ 48khz
    static constexpr int OPUS_MAX_PACKET = 4000;

    // 44100→48000リサンプリングバッファ
    std::vector<int16_t> m_resampleBuf;
    std::vector<float> m_floatBuf;
    std::vector<uint8_t> m_opusBuf;
    std::vector<int16_t> m_inputAccum; // 入力蓄積バッファ
};
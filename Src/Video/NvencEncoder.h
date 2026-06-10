#pragma once

#ifdef CreateSemaphore
#undef CreateSemaphore
#endif
#ifdef CreateMutex
#undef CreateMutex
#endif

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <cuda.h>
#include <cuda_gl_interop.h>
#include "nvEncodeAPI.h"
#include "Network/RtpSender.h" // 追加

#ifdef CreateSemaphore
#undef CreateSemaphore
#endif
#ifdef CreateMutex
#undef CreateMutex
#endif

class NvencEncoder
{
public:
    using OnEncodedCallback = std::function<void(const uint8_t *data, int size)>;

    NvencEncoder() = default;
    ~NvencEncoder() { Shutdown(); }

    
    bool Init(int width, int height, int fps, int port, OnEncodedCallback cb);
    void EncodeFrame(unsigned int glTextureID);
    void SetDestIP(const std::string &ip);
    void SetDestIPs(const std::vector<std::string> &ips);
    void SetDestPort(int port) { m_rtpSender.SetDestPort(port); }
    void Shutdown();
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
   

private:
    bool LoadNvencDll();
    bool InitCuda();
    bool CreateEncoder();
    bool AllocateBuffers();

    void ProcessOutput(int idx);

    NV_ENCODE_API_FUNCTION_LIST m_nvenc = {};
    void *m_encoder = nullptr;
    CUcontext m_cuContext = nullptr;

    static constexpr int BUFFER_COUNT = 3;

    cudaGraphicsResource_t m_cuResource = nullptr;
    unsigned int m_glTexID = 0;

    NV_ENC_INPUT_PTR m_inputBuffers[BUFFER_COUNT] = {};
    NV_ENC_OUTPUT_PTR m_outputBuffers[BUFFER_COUNT] = {};
    int m_bufferIndex = 0;

    int m_width = 0;
    int m_height = 0;
    int m_fps = 60;

    HMODULE m_nvencDll = nullptr;
    OnEncodedCallback m_callback;

    // RTP送信
    RtpSender m_rtpSender;
    bool m_rtpEnabled = false;
};
#include "NvencEncoder.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

typedef NVENCSTATUS(NVENCAPI *PFN_NvEncodeAPICreateInstance)(NV_ENCODE_API_FUNCTION_LIST *);

// ============================================================
bool NvencEncoder::LoadNvencDll()
{
    m_nvencDll = LoadLibraryA("nvEncodeAPI64.dll");
    if (!m_nvencDll)
    {
        printf("[NVENC] Failed to load nvEncodeAPI64.dll\n");
        return false;
    }

    auto createInstance = (PFN_NvEncodeAPICreateInstance)
        GetProcAddress(m_nvencDll, "NvEncodeAPICreateInstance");
    if (!createInstance)
    {
        printf("[NVENC] NvEncodeAPICreateInstance not found\n");
        return false;
    }

    m_nvenc.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NVENCSTATUS st = createInstance(&m_nvenc);
    if (st != NV_ENC_SUCCESS)
    {
        printf("[NVENC] NvEncodeAPICreateInstance failed: %d\n", st);
        return false;
    }

    printf("[NVENC] nvEncodeAPI64.dll loaded OK\n");
    return true;
}

// ============================================================
bool NvencEncoder::InitCuda()
{
    CUresult res = cuInit(0);
    if (res != CUDA_SUCCESS)
    {
        printf("[NVENC] cuInit failed: %d\n", res);
        return false;
    }

    CUdevice device;
    res = cuDeviceGet(&device, 0);
    if (res != CUDA_SUCCESS)
    {
        printf("[NVENC] cuDeviceGet failed: %d\n", res);
        return false;
    }

    // CUDA 13.1はcuCtxCreate_v4になったので古いAPIを明示的に使う
    CUctxCreateParams params = {};
    params.execAffinityParams = nullptr;
    res = cuCtxCreate(&m_cuContext, &params, 0, device);
    if (res != CUDA_SUCCESS)
    {
        printf("[NVENC] cuCtxCreate failed: %d\n", res);
        return false;
    }

    printf("[NVENC] CUDA context created\n");
    return true;
}

// ============================================================
bool NvencEncoder::CreateEncoder()
{
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS sessionParams = {};
    sessionParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    sessionParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
    sessionParams.device = m_cuContext;
    sessionParams.apiVersion = NVENCAPI_VERSION;

    NVENCSTATUS st = m_nvenc.nvEncOpenEncodeSessionEx(&sessionParams, &m_encoder);
    if (st != NV_ENC_SUCCESS)
    {
        printf("[NVENC] nvEncOpenEncodeSessionEx failed: %d\n", st);
        return false;
    }

    // プリセット設定取得
    NV_ENC_PRESET_CONFIG presetConfig = {};
    presetConfig.version = NV_ENC_PRESET_CONFIG_VER;
    presetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
    m_nvenc.nvEncGetEncodePresetConfigEx(
        m_encoder,
        NV_ENC_CODEC_H264_GUID,
        NV_ENC_PRESET_P1_GUID,
        NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY,
        &presetConfig);

    NV_ENC_CONFIG encConfig = presetConfig.presetCfg;

    // 低遅延CBR設定
    encConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
    encConfig.rcParams.averageBitRate = 3000000; // 4Mbps
    encConfig.rcParams.maxBitRate = 3000000;
    encConfig.rcParams.vbvBufferSize = 0;
    encConfig.rcParams.vbvInitialDelay = 0;
    encConfig.frameIntervalP = 1; // Bフレームなし
    encConfig.gopLength = m_fps;

    encConfig.encodeCodecConfig.h264Config.idrPeriod = m_fps;
    encConfig.encodeCodecConfig.h264Config.sliceMode = 0;
    encConfig.encodeCodecConfig.h264Config.sliceModeData = 0;

    NV_ENC_INITIALIZE_PARAMS initParams = {};
    initParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
    initParams.encodeGUID = NV_ENC_CODEC_H264_GUID;
    initParams.presetGUID = NV_ENC_PRESET_P1_GUID;
    initParams.encodeWidth = m_width;
    initParams.encodeHeight = m_height;
    initParams.darWidth = m_width;
    initParams.darHeight = m_height;
    initParams.frameRateNum = m_fps;
    initParams.frameRateDen = 1;
    initParams.enablePTD = 1;
    initParams.encodeConfig = &encConfig;
    initParams.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;

    st = m_nvenc.nvEncInitializeEncoder(m_encoder, &initParams);
    if (st != NV_ENC_SUCCESS)
    {
        printf("[NVENC] nvEncInitializeEncoder failed: %d\n", st);
        return false;
    }

    printf("[NVENC] Encoder initialized %dx%d @%dfps\n", m_width, m_height, m_fps);
    return true;
}

// ============================================================
bool NvencEncoder::AllocateBuffers()
{
    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        // 入力バッファ（CUDAメモリ）
        NV_ENC_CREATE_INPUT_BUFFER inputBuf = {};
        inputBuf.version = NV_ENC_CREATE_INPUT_BUFFER_VER;
        inputBuf.width = m_width;
        inputBuf.height = m_height;
        inputBuf.bufferFmt = NV_ENC_BUFFER_FORMAT_ABGR; // RGBA

        NVENCSTATUS st = m_nvenc.nvEncCreateInputBuffer(m_encoder, &inputBuf);
        if (st != NV_ENC_SUCCESS)
        {
            printf("[NVENC] nvEncCreateInputBuffer[%d] failed: %d\n", i, st);
            return false;
        }
        m_inputBuffers[i] = inputBuf.inputBuffer;

        // 出力バッファ
        NV_ENC_CREATE_BITSTREAM_BUFFER outputBuf = {};
        outputBuf.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;

        st = m_nvenc.nvEncCreateBitstreamBuffer(m_encoder, &outputBuf);
        if (st != NV_ENC_SUCCESS)
        {
            printf("[NVENC] nvEncCreateBitstreamBuffer[%d] failed: %d\n", i, st);
            return false;
        }
        m_outputBuffers[i] = outputBuf.bitstreamBuffer;
    }

    printf("[NVENC] Buffers allocated (%d)\n", BUFFER_COUNT);
    return true;
}

// ============================================================
bool NvencEncoder::Init(int width, int height, int fps, int port, OnEncodedCallback cb)
{
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_callback = cb;

    if (!LoadNvencDll())
        return false;
    if (!InitCuda())
        return false;
    if (!CreateEncoder())
        return false;
    if (!AllocateBuffers())
        return false;

    m_rtpEnabled = m_rtpSender.Init("127.0.0.1", port);

    printf("[NVENC] Init complete\n");
    return true;
}

// ============================================================
void NvencEncoder::EncodeFrame(unsigned int glTextureID)
{
    if (!m_encoder)
        return;

    int idx = m_bufferIndex % BUFFER_COUNT;

    // OpenGLテクスチャをCUDAに登録（初回 or テクスチャ変更時）
    if (m_glTexID != glTextureID)
    {
        if (m_cuResource)
        {
            cudaGraphicsUnregisterResource(m_cuResource);
            m_cuResource = nullptr;
        }
        cudaError_t err = cudaGraphicsGLRegisterImage(
            &m_cuResource,
            glTextureID,
            GL_TEXTURE_2D,
            cudaGraphicsRegisterFlagsReadOnly);
        if (err != cudaSuccess)
        {
            printf("[NVENC] cudaGraphicsGLRegisterImage failed: %d\n", err);
            return;
        }
        m_glTexID = glTextureID;
    }

    // OpenGLテクスチャをCUDAにマップ
    cudaGraphicsMapResources(1, &m_cuResource, 0);

    cudaArray_t cuArray;
    cudaGraphicsSubResourceGetMappedArray(&cuArray, m_cuResource, 0, 0);

    // NVENCの入力バッファにロック
    NV_ENC_LOCK_INPUT_BUFFER lockParams = {};
    lockParams.version = NV_ENC_LOCK_INPUT_BUFFER_VER;
    lockParams.inputBuffer = m_inputBuffers[idx];

    NVENCSTATUS st = m_nvenc.nvEncLockInputBuffer(m_encoder, &lockParams);
    if (st != NV_ENC_SUCCESS)
    {
        cudaGraphicsUnmapResources(1, &m_cuResource, 0);
        return;
    }

    // CUDAArrayからNVENC入力バッファへGPU上でコピー
    cudaMemcpy2DFromArray(
        lockParams.bufferDataPtr,  // dst
        lockParams.pitch,          // dst pitch
        cuArray,                   // src
        0, 0,                      // src offset
        m_width * 4,               // width bytes (RGBA)
        m_height,                  // height
        cudaMemcpyDeviceToDevice); // GPU→GPU

    m_nvenc.nvEncUnlockInputBuffer(m_encoder, m_inputBuffers[idx]);
    cudaGraphicsUnmapResources(1, &m_cuResource, 0);

    // エンコード
    NV_ENC_PIC_PARAMS picParams = {};
    picParams.version = NV_ENC_PIC_PARAMS_VER;
    picParams.inputBuffer = m_inputBuffers[idx];
    picParams.outputBitstream = m_outputBuffers[idx];
    picParams.bufferFmt = NV_ENC_BUFFER_FORMAT_ABGR;
    picParams.inputWidth = m_width;
    picParams.inputHeight = m_height;
    picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;

    // 最初の数フレームはIDRフレームを強制
    if (m_bufferIndex < 3)
        picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;
    else
        picParams.encodePicFlags = NV_ENC_PIC_FLAG_OUTPUT_SPSPPS; // 毎フレームSPS/PPS付加

    m_nvenc.nvEncEncodePicture(m_encoder, &picParams);

    ProcessOutput(idx);

    m_bufferIndex++;
}

// ============================================================
void NvencEncoder::ProcessOutput(int idx)
{
    NV_ENC_LOCK_BITSTREAM lockBitstream = {};
    lockBitstream.version = NV_ENC_LOCK_BITSTREAM_VER;
    lockBitstream.outputBitstream = m_outputBuffers[idx];
    lockBitstream.doNotWait = 0;

    NVENCSTATUS st = m_nvenc.nvEncLockBitstream(m_encoder, &lockBitstream);
    if (st == NV_ENC_SUCCESS)
    {
        const uint8_t *data = (const uint8_t *)lockBitstream.bitstreamBufferPtr;
        int size = (int)lockBitstream.bitstreamSizeInBytes;

        // 先頭NALタイプを表示
        int offset = 0;
        while (offset < size - 4)
        {
            if (data[offset] == 0 && data[offset + 1] == 0 &&
                data[offset + 2] == 0 && data[offset + 3] == 1)
            {
                int nalType = data[offset + 4] & 0x1F;
                //printf("[NVENC] NAL type=%d at offset=%d\n", nalType, offset);
                offset += 4;
            }
            else
                offset++;
            if (offset > 100)
                break; // 先頭付近だけ見る
        }
        if (m_callback)
            m_callback(
                (const uint8_t *)lockBitstream.bitstreamBufferPtr,
                (int)lockBitstream.bitstreamSizeInBytes);

        // RTP送信
        if (m_rtpEnabled)
            m_rtpSender.Send(
                (const uint8_t *)lockBitstream.bitstreamBufferPtr,
                (int)lockBitstream.bitstreamSizeInBytes);

        m_nvenc.nvEncUnlockBitstream(m_encoder, m_outputBuffers[idx]);
    }
}

// ============================================================
void NvencEncoder::Shutdown()
{
    if (!m_encoder)
        return;

    if (m_cuResource)
    {
        cudaGraphicsUnregisterResource(m_cuResource);
        m_cuResource = nullptr;
    }

    // フラッシュ
    NV_ENC_PIC_PARAMS flushParams = {};
    flushParams.version = NV_ENC_PIC_PARAMS_VER;
    flushParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
    m_nvenc.nvEncEncodePicture(m_encoder, &flushParams);

    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        if (m_inputBuffers[i])
            m_nvenc.nvEncDestroyInputBuffer(m_encoder, m_inputBuffers[i]);
        if (m_outputBuffers[i])
            m_nvenc.nvEncDestroyBitstreamBuffer(m_encoder, m_outputBuffers[i]);
    }

    m_nvenc.nvEncDestroyEncoder(m_encoder);
    m_encoder = nullptr;

    if (m_nvencDll)
    {
        FreeLibrary(m_nvencDll);
        m_nvencDll = nullptr;
    }

    printf("[NVENC] Shutdown complete\n");
}
void NvencEncoder::SetDestIP(const std::string& ip)
{
    m_rtpSender.SetDestIP(ip);  // Shutdown/Initしない
    printf("[NVENC] IP changed to %s\n", ip.c_str());
}

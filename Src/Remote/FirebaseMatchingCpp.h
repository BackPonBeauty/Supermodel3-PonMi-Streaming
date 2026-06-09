/**
 * FirebaseMatchingCpp.h
 *
 * Firebase Realtime Database REST API ラッパー（WinHTTP使用）
 *
 * VB.NET の FirebaseMatching.vb と同等の機能を提供:
 *   - 匿名認証（Firebase Auth REST API）
 *   - ホスト情報の登録・更新
 *   - ハートビート（2分ごと）
 *   - 外部IPアドレス取得
 *   - 古いホストエントリのクリーンアップ
 */
#pragma once

#ifdef SUPERMODEL_WIN32

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <map>

#pragma comment(lib, "winhttp.lib")

// スロット情報（VB.NET の SlotInfo クラスと対応）
struct SlotInfoCpp
{
    int  xinputPort;
    int  videoPort;
    int  audioPort;
    bool available;
};

// ホスト情報（VB.NET の HostInfo クラスと対応）
struct HostInfoCpp
{
    long long                    timestamp;
    std::string                  ip;
    std::string                  gametitle; // 起動中のROM名（例: spikeofe）
    std::map<int, SlotInfoCpp>   slots; // key: スロット番号 1〜4
};

using FirebaseStatusCallback = std::function<void(const std::string& message)>;

class FirebaseMatchingCpp
{
public:
    // Firebase プロジェクト設定（VB.NET の FirebaseMatching.vb と一致）
    static constexpr const char* API_KEY     = "your api key here";
    static constexpr const char* DB_URL      = "https://your-project-id-default-rtdb.firebaseio.com";
    static constexpr const char* AUTH_DOMAIN = "your-project-id.firebaseapp.com";

    // ポート定義（VB.NET の SlotPorts と一致）
    static constexpr int SLOT_XINPUT_PORT[5] = { 0, 5000, 5004, 5008, 5012 };
    static constexpr int SLOT_VIDEO_PORT[5]  = { 0, 5002, 5006, 5010, 5014 };
    static constexpr int SLOT_AUDIO_PORT[5]  = { 0, 5003, 5007, 5011, 5015 };

    FirebaseMatchingCpp();
    ~FirebaseMatchingCpp();

    // 匿名認証を行い、DBクライアントを初期化
    bool Initialize(FirebaseStatusCallback statusCb = nullptr);
    void Shutdown();

    // ホスト情報をFirebaseに登録（PUT=新規作成 / PATCH=スロット更新）
    // キー = SanitizeKeyFromIp(externalIp) で自動決定
    bool RegisterHost(const std::string& externalIp,
                      const bool availableSlots[5],
                      int linkplay,
                      const std::string& gameTitle = "");

    // ホスト登録を解除
    bool UnregisterHost(const std::string& hostId);
    bool PatchSlotAvailable(const std::string& hostId, int linkplay, bool available);

    // 古いホストエントリ（10分以上更新なし）を削除
    bool CleanupStaleHosts();

    // 外部IPアドレスを取得（api.ipify.org使用）
    static std::string GetExternalIp();

    // ハートビート開始（2分ごとにタイムスタンプ更新）
    void StartHeartbeat(const std::string& hostId);
    void StopHeartbeat();

    bool IsInitialized() const { return m_initialized; }

private:
    // WinHTTP でHTTPリクエストを送信
    std::string HttpPost(const std::wstring& host, const std::wstring& path,
                         const std::string& jsonBody, bool useHttps = true);
    std::string HttpGet(const std::wstring& host, const std::wstring& path,
                        const std::string& authToken = "", bool useHttps = true);
    std::string HttpPut(const std::wstring& host, const std::wstring& path,
                        const std::string& jsonBody,
                        const std::string& authToken = "", bool useHttps = true);
    std::string HttpPatch(const std::wstring& host, const std::wstring& path,
                          const std::string& jsonBody,
                          const std::string& authToken = "", bool useHttps = true);
    std::string HttpDelete(const std::wstring& host, const std::wstring& path,
                           const std::string& authToken = "", bool useHttps = true);

    // IPアドレス→Firebaseキー変換（ドット等を '-' に置換）
    static std::string SanitizeKeyFromIp(const std::string& ip);

    // Firebase Auth 匿名ログイン
    bool SignInAnonymously();

    // タイムスタンプ（Unix秒）を取得
    static long long GetUnixTimestamp();

    // JSON ユーティリティ（軽量実装）
    static std::string MakeHostJson(const HostInfoCpp& info);
    static std::string EscapeJson(const std::string& s);

    bool                  m_initialized = false;
    std::string           m_idToken;     // Firebase IDトークン
    std::string           m_currentHostId;

    // ハートビートスレッド
    std::thread           m_heartbeatThread;
    std::atomic<bool>     m_heartbeatRunning{ false };

    FirebaseStatusCallback m_statusCb;

    void HeartbeatThread(std::string hostId);
};

#endif // SUPERMODEL_WIN32

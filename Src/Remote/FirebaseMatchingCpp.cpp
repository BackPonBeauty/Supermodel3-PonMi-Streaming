/**
 * FirebaseMatchingCpp.cpp
 *
 * Firebase Realtime Database REST API 実装（WinHTTP使用）
 */

#ifdef SUPERMODEL_WIN32

#include "FirebaseMatchingCpp.h"
#include <winhttp.h>
#include <sstream>
#include <ctime>
#include <cstdio>
#include <chrono>

constexpr int HEARTBEAT_INTERVAL_MS = 120000; // 2分
constexpr int TIMEOUT_MINUTES = 10;

// ---------------------------------------------------------------------------
// コンストラクタ / デストラクタ
// ---------------------------------------------------------------------------
FirebaseMatchingCpp::FirebaseMatchingCpp() {}

FirebaseMatchingCpp::~FirebaseMatchingCpp()
{
    Shutdown();
}

void FirebaseMatchingCpp::Shutdown()
{
    StopHeartbeat();
    if (!m_currentHostId.empty())
        UnregisterHost(m_currentHostId);
    m_initialized = false;
    m_idToken.clear();
}

// ---------------------------------------------------------------------------
// 初期化（匿名認証）
// ---------------------------------------------------------------------------
bool FirebaseMatchingCpp::Initialize(FirebaseStatusCallback statusCb)
{
    m_statusCb = statusCb;
    if (m_statusCb)
        m_statusCb("Firebase: Initializing...");

    if (!SignInAnonymously())
    {
        if (m_statusCb)
            m_statusCb("Firebase: Fault");
        return false;
    }

    m_initialized = true;
    if (m_statusCb)
        m_statusCb("Firebase: Ready");
    return true;
}

// ---------------------------------------------------------------------------
// Firebase Auth 匿名ログイン
// ---------------------------------------------------------------------------
bool FirebaseMatchingCpp::SignInAnonymously()
{
    // POST https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=API_KEY
    std::string path = "/v1/accounts:signUp?key=";
    path += API_KEY; 
    //"AIzaSyCjuWjnnKAPrIhe8LrzosuXcCuMSW9x7Zs";

    std::string body = "{\"returnSecureToken\":true}";

    std::string response = HttpPost(
        L"identitytoolkit.googleapis.com",
        std::wstring(path.begin(), path.end()),
        body, true);

  //  printf("[Firebase] SignIn response: %s\n", response.c_str());

    if (response.empty())
        return false;

    // idToken を JSON から取得（軽量パース）
    const std::string key = "\"idToken\":\"";
    const std::string key1 = "\"idToken\":\"";
    const std::string key2 = "\"idToken\": \"";

    size_t pos = response.find(key1);
    size_t keyLen = key1.size();
    if (pos == std::string::npos)
    {
        pos = response.find(key2);
        keyLen = key2.size();
    }
    if (pos == std::string::npos)
        return false;

    pos += keyLen;
    size_t end = response.find('"', pos);
    if (end == std::string::npos)
        return false;

    m_idToken = response.substr(pos, end - pos);
    return !m_idToken.empty();
}

// ---------------------------------------------------------------------------
// ホスト登録
// ---------------------------------------------------------------------------
bool FirebaseMatchingCpp::RegisterHost(const std::string &externalIp,
                                       const bool availableSlots[5],
                                       int linkplay,
                                       const std::string &gameTitle)
{
    // キー = IPをサニタイズ（ドット→'-'）
    std::string key = SanitizeKeyFromIp(externalIp);
    m_currentHostId = key;

    std::wstring dbHost = L"supermodel3-8343f-default-rtdb.asia-southeast1.firebasedatabase.app";

    // GET でレコードが存在するか確認
    std::string getPath = "/hosts/" + key + ".json?auth=" + m_idToken;
    std::string existing = HttpGet(dbHost, std::wstring(getPath.begin(), getPath.end()), m_idToken);
    bool exists = !existing.empty() && existing != "null";

    if (!exists)
    {
        // ----- PUT: 新規レコード作成 -----
        // linkplayスロット: 物理コントローラーあり=false, 仮想=true
        // その他のスロット: 常にtrue
        HostInfoCpp info;
        info.timestamp = GetUnixTimestamp();
        info.ip = externalIp;
        info.gametitle = gameTitle; // ROM名（例: spikeofe）

        for (int slot = 1; slot <= 4; slot++)
        {
            SlotInfoCpp s;
            s.xinputPort = SLOT_XINPUT_PORT[slot];
            s.videoPort = SLOT_VIDEO_PORT[slot];
            s.audioPort = SLOT_AUDIO_PORT[slot];
            s.available = (slot == linkplay) ? availableSlots[slot] : false;
            info.slots[slot] = s;
        }

        std::string putPath = "/hosts/" + key + ".json?auth=" + m_idToken;
        std::string body = MakeHostJson(info);
        std::string resp = HttpPut(dbHost, std::wstring(putPath.begin(), putPath.end()), body, m_idToken);

        printf("[Firebase] PUT (new record) key=%s linkplay=%d\n", key.c_str(), linkplay);
        if (m_statusCb)
            m_statusCb("Firebase: Host registered (PUT) key=" + key);
        return !resp.empty();
    }
   else
{
    // PATCH: 自スロットのavailableと timestamp のみ更新
    long long ts = GetUnixTimestamp();
    std::string slotKey = "slot" + std::to_string(linkplay);

    // /hosts/{key}/slot2.json にPATCH → 他スロットに影響なし
    std::string patchPath = "/hosts/" + key + "/" + slotKey + ".json?auth=" + m_idToken;
    std::ostringstream ss;
    ss << "{\"available\":" << (availableSlots[linkplay] ? "true" : "false") << "}";
    HttpPatch(dbHost, std::wstring(patchPath.begin(), patchPath.end()), ss.str(), m_idToken);

    // timestamp を別途PUT
    std::string tsPath = "/hosts/" + key + "/timestamp.json?auth=" + m_idToken;
    HttpPut(dbHost, std::wstring(tsPath.begin(), tsPath.end()), std::to_string(ts), m_idToken);

    printf("[Firebase] PATCH slot%d available=%s key=%s\n",
           linkplay, availableSlots[linkplay] ? "true" : "false", key.c_str());
    if (m_statusCb)
        m_statusCb("Firebase: Slot" + std::to_string(linkplay) + " updated (PATCH)");
    return true;
}
}

bool FirebaseMatchingCpp::UnregisterHost(const std::string &hostId)
{
    if (hostId.empty() || m_idToken.empty())
        return false;

    std::string path = "/hosts/" + hostId + ".json?auth=" + m_idToken;
    std::wstring wpath(path.begin(), path.end());
    std::wstring dbHost = L"supermodel3-8343f-default-rtdb.asia-southeast1.firebasedatabase.app";

    HttpDelete(dbHost, wpath, m_idToken);
    m_currentHostId.clear();
    return true;
}

bool FirebaseMatchingCpp::PatchSlotAvailable(const std::string& hostId, int linkplay, bool available)
{
    if (hostId.empty() || m_idToken.empty()) return false;

    std::string slotKey = "slot" + std::to_string(linkplay);
    std::string path = "/hosts/" + hostId + "/" + slotKey + ".json?auth=" + m_idToken;
    std::wstring wpath(path.begin(), path.end());
    std::wstring dbHost = L"supermodel3-8343f-default-rtdb.asia-southeast1.firebasedatabase.app";

    std::string body = std::string("{\"available\":") + (available ? "true" : "false") + "}";
    HttpPatch(dbHost, wpath, body, m_idToken);
    printf("[Firebase] PatchSlotAvailable slot%d=%s\n", linkplay, available ? "true" : "false");
    return true;
}

// ---------------------------------------------------------------------------
// 古いホストのクリーンアップ（10分以上更新なし）
// ---------------------------------------------------------------------------
bool FirebaseMatchingCpp::CleanupStaleHosts()
{
    if (m_idToken.empty())
        return false;

    std::string path = "/hosts.json?auth=" + m_idToken;
    std::wstring wpath(path.begin(), path.end());
    std::wstring dbHost = L"supermodel3-8343f-default-rtdb.asia-southeast1.firebasedatabase.app";

    std::string resp = HttpGet(dbHost, wpath, m_idToken);
    if (resp.empty() || resp == "null")
        return true;

    long long now = GetUnixTimestamp();                                   // ミリ秒
    long long threshold = now - (long long)(TIMEOUT_MINUTES * 60 * 1000); // 秒→ミリ秒

    // 簡易パース: "timestamp": 数値 を探してしきい値以下のホストIDを削除
    size_t pos = 0;
    while (true)
    {
        // ホストIDを検索 ("xxxxxxxx": {)
        pos = resp.find("\"timestamp\":", pos);
        if (pos == std::string::npos)
            break;

        pos += 12; // "timestamp": の長さ
        while (pos < resp.size() && (resp[pos] == ' ' || resp[pos] == '\t'))
            pos++;

        long long ts = 0;
        size_t numEnd = pos;
        while (numEnd < resp.size() && isdigit(resp[numEnd]))
            numEnd++;
        if (numEnd > pos)
        {
            ts = std::stoll(resp.substr(pos, numEnd - pos));
        }

        if (ts > 0 && ts < threshold)
        {
            // 対応するホストIDを逆向きに探す
            size_t braceOpen = resp.rfind('"', pos - 15);
            if (braceOpen != std::string::npos)
            {
                size_t idEnd = resp.rfind('"', braceOpen - 1);
                size_t idStart = resp.rfind('"', idEnd - 1);
                if (idStart != std::string::npos)
                {
                    std::string staleId = resp.substr(idStart + 1, idEnd - idStart - 1);
                    if (!staleId.empty())
                    {
                        std::string delPath = "/hosts/" + staleId + ".json?auth=" + m_idToken;
                        std::wstring wDelPath(delPath.begin(), delPath.end());
                        HttpDelete(dbHost, wDelPath, m_idToken);
                        printf("[Firebase] Stale host removed: %s (ts=%lld)\n", staleId.c_str(), ts);
                    }
                }
            }
        }
        pos = numEnd;
    }

    return true;
}

// ---------------------------------------------------------------------------
// 外部IPアドレス取得
// ---------------------------------------------------------------------------
std::string FirebaseMatchingCpp::GetExternalIp()
{
    // GET https://api.ipify.org
    HINTERNET session = WinHttpOpen(L"Supermodel3/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return "";

    HINTERNET conn = WinHttpConnect(session, L"api.ipify.org", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!conn)
    {
        WinHttpCloseHandle(session);
        return "";
    }

    HINTERNET req = WinHttpOpenRequest(conn, L"GET", L"/", nullptr,
                                       WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       WINHTTP_FLAG_SECURE);
    if (!req)
    {
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        return "";
    }

    std::string result;
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr))
    {
        DWORD size = 0;
        char buf[64] = {};
        WinHttpQueryDataAvailable(req, &size);
        if (size > 0 && size < sizeof(buf) - 1)
        {
            DWORD read = 0;
            WinHttpReadData(req, buf, size, &read);
            result = std::string(buf, read);
        }
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return result;
}

// ---------------------------------------------------------------------------
// ハートビート
// ---------------------------------------------------------------------------
void FirebaseMatchingCpp::StartHeartbeat(const std::string &hostId)
{
    StopHeartbeat();
    m_heartbeatRunning.store(true);
    m_heartbeatThread = std::thread(&FirebaseMatchingCpp::HeartbeatThread, this, hostId);
}

void FirebaseMatchingCpp::StopHeartbeat()
{
    m_heartbeatRunning.store(false);
    if (m_heartbeatThread.joinable())
        m_heartbeatThread.join();
}

void FirebaseMatchingCpp::HeartbeatThread(std::string hostId)
{
    while (m_heartbeatRunning.load())
    {
        // HEARTBEAT_INTERVAL_MS 待機（100msごとにrunningチェック）
        for (int i = 0; i < HEARTBEAT_INTERVAL_MS / 100 && m_heartbeatRunning.load(); i++)
            Sleep(100);

        if (!m_heartbeatRunning.load())
            break;

        // タイムスタンプを更新
        std::string path = "/hosts/" + hostId + "/timestamp.json?auth=" + m_idToken;
        std::wstring wpath(path.begin(), path.end());
        std::wstring dbHost = L"supermodel3-8343f-default-rtdb.asia-southeast1.firebasedatabase.app";

        long long ts = GetUnixTimestamp();
        std::string body = std::to_string(ts);
        HttpPut(dbHost, wpath, body, m_idToken);
        printf("[Firebase] Heartbeat updated: %s ts=%lld\n", hostId.c_str(), ts);
    }
}

// ---------------------------------------------------------------------------
// WinHTTP ヘルパー関数
// ---------------------------------------------------------------------------
std::string FirebaseMatchingCpp::HttpPost(const std::wstring &host, const std::wstring &path,
                                          const std::string &jsonBody, bool useHttps)
{
    HINTERNET session = WinHttpOpen(L"Supermodel3/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return "";

    INTERNET_PORT port = useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET conn = WinHttpConnect(session, host.c_str(), port, 0);
    if (!conn)
    {
        WinHttpCloseHandle(session);
        return "";
    }

    DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, L"POST", path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req)
    {
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        return "";
    }

    std::wstring contentType = L"Content-Type: application/json\r\n";
    WinHttpAddRequestHeaders(req, contentType.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    std::string result;
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.size(),
                           (DWORD)jsonBody.size(), 0) &&
        WinHttpReceiveResponse(req, nullptr))
    {
        DWORD size = 0;
        while (WinHttpQueryDataAvailable(req, &size) && size > 0)
        {
            std::string chunk(size, '\0');
            DWORD read = 0;
            WinHttpReadData(req, &chunk[0], size, &read);
            result.append(chunk, 0, read);
        }
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return result;
}

std::string FirebaseMatchingCpp::HttpGet(const std::wstring &host, const std::wstring &path,
                                         const std::string & /*authToken*/, bool useHttps)
{
    HINTERNET session = WinHttpOpen(L"Supermodel3/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return "";

    INTERNET_PORT port = useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET conn = WinHttpConnect(session, host.c_str(), port, 0);
    if (!conn)
    {
        WinHttpCloseHandle(session);
        return "";
    }

    DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    std::string result;
    if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr))
    {
        DWORD size = 0;
        while (WinHttpQueryDataAvailable(req, &size) && size > 0)
        {
            std::string chunk(size, '\0');
            DWORD read = 0;
            WinHttpReadData(req, &chunk[0], size, &read);
            result.append(chunk, 0, read);
        }
    }

    if (req)
        WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return result;
}

std::string FirebaseMatchingCpp::HttpPut(const std::wstring &host, const std::wstring &path,
                                         const std::string &jsonBody,
                                         const std::string & /*authToken*/, bool useHttps)
{
    HINTERNET session = WinHttpOpen(L"Supermodel3/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return "";

    INTERNET_PORT port = useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET conn = WinHttpConnect(session, host.c_str(), port, 0);
    if (!conn)
    {
        WinHttpCloseHandle(session);
        return "";
    }

    DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, L"PUT", path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    std::string result;
    if (req)
    {
        std::wstring ct = L"Content-Type: application/json\r\n";
        WinHttpAddRequestHeaders(req, ct.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.size(),
                               (DWORD)jsonBody.size(), 0) &&
            WinHttpReceiveResponse(req, nullptr))
        {
            DWORD size = 0;
            while (WinHttpQueryDataAvailable(req, &size) && size > 0)
            {
                std::string chunk(size, '\0');
                DWORD read = 0;
                WinHttpReadData(req, &chunk[0], size, &read);
                result.append(chunk, 0, read);
            }
        }
        WinHttpCloseHandle(req);
    }

    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return result;
}

// PATCH
std::string FirebaseMatchingCpp::HttpPatch(const std::wstring &host, const std::wstring &path,
                                           const std::string &jsonBody,
                                           const std::string & /*authToken*/, bool useHttps)
{
    HINTERNET session = WinHttpOpen(L"Supermodel3/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return "";

    INTERNET_PORT port = useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET conn = WinHttpConnect(session, host.c_str(), port, 0);
    if (!conn)
    {
        WinHttpCloseHandle(session);
        return "";
    }

    DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, L"PATCH", path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    std::string result;
    if (req)
    {
        std::wstring ct = L"Content-Type: application/json\r\n";
        WinHttpAddRequestHeaders(req, ct.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.size(),
                               (DWORD)jsonBody.size(), 0) &&
            WinHttpReceiveResponse(req, nullptr))
        {
            DWORD size = 0;
            while (WinHttpQueryDataAvailable(req, &size) && size > 0)
            {
                std::string chunk(size, '\0');
                DWORD read = 0;
                WinHttpReadData(req, &chunk[0], size, &read);
                result.append(chunk, 0, read);
            }
        }
        WinHttpCloseHandle(req);
    }

    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return result;
}

std::string FirebaseMatchingCpp::HttpDelete(const std::wstring &host, const std::wstring &path,
                                            const std::string & /*authToken*/, bool useHttps)
{
    HINTERNET session = WinHttpOpen(L"Supermodel3/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return "";

    INTERNET_PORT port = useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET conn = WinHttpConnect(session, host.c_str(), port, 0);
    if (!conn)
    {
        WinHttpCloseHandle(session);
        return "";
    }

    DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, L"DELETE", path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    std::string result;
    if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr))
    {
        result = "ok";
    }

    if (req)
        WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return result;
}

// ---------------------------------------------------------------------------
// ユーティリティ
// ---------------------------------------------------------------------------
long long FirebaseMatchingCpp::GetUnixTimestamp()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch())
        .count();
}

std::string FirebaseMatchingCpp::EscapeJson(const std::string &s)
{
    std::string out;
    for (char c : s)
    {
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else
            out += c;
    }
    return out;
}

// IPアドレス→Firebaseキー変換（. # $ / [ ] を '-' に置換）
std::string FirebaseMatchingCpp::SanitizeKeyFromIp(const std::string &ip)
{
    std::string key = ip;
    for (char &c : key)
    {
        if (c == '.' || c == '#' || c == '$' ||
            c == '/' || c == '[' || c == ']')
            c = '-';
    }
    return key;
}

std::string FirebaseMatchingCpp::MakeHostJson(const HostInfoCpp &info)
{
    std::ostringstream ss;
    ss << "{";
    ss << "\"timestamp\":" << info.timestamp << ",";
    ss << "\"ip\":\"" << EscapeJson(info.ip) << "\",";
    if (!info.gametitle.empty())
        ss << "\"gametitle\":\"" << EscapeJson(info.gametitle) << "\",";

    for (const auto &kv : info.slots)
    {
        const SlotInfoCpp &s = kv.second;
        ss << "\"slot" << kv.first << "\":{";
        ss << "\"xinput\":" << s.xinputPort << ",";
        ss << "\"video\":" << s.videoPort << ",";
        ss << "\"audio\":" << s.audioPort << ",";
        ss << "\"available\":" << (s.available ? "true" : "false");
        ss << "},";
    }

    // 末尾のカンマを除去
    std::string result = ss.str();
    if (result.back() == ',') result.pop_back();
    result += "}";
    return result;
}

#endif // SUPERMODEL_WIN32

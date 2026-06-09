/**
 * RemoteSlotManager.cpp
 *
 * P1〜P4スロット管理の実装
 */

#ifdef SUPERMODEL_WIN32

#include "RemoteSlotManager.h"
#include <xinput.h>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <random>
#include "UPnPHelper.h"

// #pragma comment(lib, "xinput.lib")

static HMODULE s_xinputDll = nullptr;
typedef DWORD(WINAPI *PFN_XInputGetState)(DWORD, XINPUT_STATE *);
static PFN_XInputGetState s_XInputGetState = nullptr;

static void LoadXInputForRemote()
{
    if (s_xinputDll)
        return;
    const char *dlls[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    for (auto dll : dlls)
    {
        s_xinputDll = LoadLibraryA(dll);
        if (s_xinputDll)
        {
            s_XInputGetState = (PFN_XInputGetState)GetProcAddress(s_xinputDll, "XInputGetState");
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// XInput 物理コントローラー検出
// ---------------------------------------------------------------------------
bool RemoteSlotManager::IsXInputConnected(int userIndex)
{
    LoadXInputForRemote();
    if (!s_XInputGetState)
        return false;
    XINPUT_STATE state = {};
    DWORD result = s_XInputGetState(userIndex, &state);
    return result == ERROR_SUCCESS;
}

// ---------------------------------------------------------------------------
// コンストラクタ / デストラクタ
// ---------------------------------------------------------------------------
RemoteSlotManager::RemoteSlotManager()
{
    for (int i = 1; i <= 4; i++)
    {
        m_slots[i].mode = SlotMode::LOCAL;
        m_slots[i].isPhysical = false;
        m_slots[i].isConnected = false;
        m_slots[i].label = "";
    }
}

RemoteSlotManager::~RemoteSlotManager()
{
    Shutdown();
}

// ---------------------------------------------------------------------------
// 初期化
// ---------------------------------------------------------------------------
bool RemoteSlotManager::Initialize(SlotChangedCallback slotCb,
                                   StatusCallback statusCb)
{
    m_slotCb = slotCb;
    m_statusCb = statusCb;

    NotifyStatus("初期化中...");

    // Winsock 初期化
    XinputReceiver::InitWinsock();

    // ViGEm 初期化
    if (!m_vigem.Initialize())
    {
        NotifyStatus("warning: " + m_vigem.GetStatusMessage());
        // ViGEm失敗でも続行（ローカルモードのみ動作）
    }
    else
    {
        NotifyStatus(m_vigem.GetStatusMessage());
    }

    // 物理コントローラー検出
    DetectPhysicalControllers();

    // ホストID生成（8文字ランダム）
    m_hostId = GenerateHostId();

    // 外部IPアドレス取得（別スレッドで非同期に）
    std::thread([this]()
                {
        m_externalIp = FirebaseMatchingCpp::GetExternalIp();
        if (!m_externalIp.empty())
            NotifyStatus("output IP: " + m_externalIp); })
        .detach();

    // Firebase 初期化（別スレッドで非同期に）
    std::thread([this]()
                {
        auto cb = [this](const std::string& msg) { NotifyStatus(msg); };
        if (m_firebase.Initialize(cb))
        {
            m_firebase.CleanupStaleHosts();
            UpdateAvailableSlots(); // 初期登録
            m_firebase.StartHeartbeat(m_hostId);
        } })
        .detach();

    NotifyStatus("Ready");
    return true;
}

void RemoteSlotManager::Shutdown()
{
    if (m_linkplay >= 1 && m_linkplay <= 4)
        UPnPHelper::CloseStreamingPorts(m_linkplay);
    
    m_xinput.StopAll();
    for (int i = 1; i <= 4; i++)
        m_vigem.RemoveController(i);
    m_virtualControllerAdded = false;
    m_firebase.Shutdown();
    m_vigem.Shutdown();
    XinputReceiver::CleanupWinsock();
}

// ---------------------------------------------------------------------------
// 単独 ViGEm 操作（GUI連動・起動時使用）
// ---------------------------------------------------------------------------
bool RemoteSlotManager::InitViGEm()
{
    if (m_vigem.IsInitialized())
        return true; // 既に初期化済み

    // Winsock は先に初期化
    XinputReceiver::InitWinsock();

    if (!m_vigem.Initialize())
    {
        printf("[RemoteSlotManager] InitViGEm failed: %s\n", m_vigem.GetStatusMessage().c_str());
        return false;
    }
    printf("[RemoteSlotManager] InitViGEm OK\n");
    return true;
}

bool RemoteSlotManager::AddVirtualController()
{
    if (m_virtualControllerAdded)
    {
        printf("[RemoteSlotManager] AddVirtualController: already added\n");
        return true;
    }
    if (!m_vigem.IsInitialized())
    {
        if (!InitViGEm())
            return false;
    }
    if (!m_vigem.AddController(1))
    {
        printf("[RemoteSlotManager] AddVirtualController failed: %s\n", m_vigem.GetStatusMessage().c_str());
        return false;
    }
    m_virtualControllerAdded = true;
    printf("[RemoteSlotManager] AddVirtualController: slot 1 created\n");
    return true;
}

void RemoteSlotManager::RemoveVirtualController()
{
    if (!m_virtualControllerAdded)
        return;
    m_xinput.StopListening(1);
    m_vigem.RemoveController(1);
    m_virtualControllerAdded = false;
    printf("[RemoteSlotManager] RemoveVirtualController: slot 1 removed\n");
}

bool RemoteSlotManager::StartListening(int linkplay)
{
    if (linkplay < 1 || linkplay > 4)
    {
        printf("[RemoteSlotManager] StartListening: invalid linkplay=%d\n", linkplay);
        return false;
    }
    if (!m_virtualControllerAdded)
    {
        printf("[RemoteSlotManager] StartListening: no virtual controller, call AddVirtualController first\n");
        return false;
    }

    int port = 5000 + (linkplay - 1) * 4;
    m_linkplay = linkplay; // INI値を保存

    // UPnPポート開放（非同期）
    std::thread([linkplay]() {
        UPnPHelper::OpenStreamingPorts(linkplay);
    }).detach();


    auto cb = [this](int s, const XInputPacket &p)
    { OnXInputReceived(s, p); };
    m_xinput.StartListening(1, port, cb);

    m_slots[1].mode = SlotMode::REMOTE;
    m_slots[1].isConnected = false;
    m_slots[1].label = "Standing by... (port " + std::to_string(port) + ")";
    NotifySlotChanged(1);

    printf("[RemoteSlotManager] StartListening: linkplay=%d port=%d\n", linkplay, port);
    return true;
}

// ---------------------------------------------------------------------------
// Firebase スレッドを Launch 時に起動（Initialize() とは独立）
// ---------------------------------------------------------------------------
void RemoteSlotManager::StartFirebaseAsync(const std::string &gameTitle)
{
    if (m_hostId.empty())
        m_hostId = GenerateHostId();

    m_gameTitle = gameTitle; // ROM名を保存
    printf("[RemoteSlotManager] StartFirebaseAsync: hostId=%s gameTitle=%s\n",
           m_hostId.c_str(), m_gameTitle.c_str());

    // 外部IP取得 → Firebase初期化・登録・ハートビートを1スレッドで順番に実行
    std::thread([this]()
                {
        // 外部IP取得を先に行う
        m_externalIp = FirebaseMatchingCpp::GetExternalIp();
        printf("[RemoteSlotManager] External IP: %s\n", m_externalIp.c_str());

        m_hostId = m_externalIp;
for (char& c : m_hostId)
    if (c == '.') c = '-';

printf("[RemoteSlotManager] HostId: %s\n", m_hostId.c_str());

        if (m_externalIp.empty() || m_externalIp == "127.0.0.1")
        {
            printf("[RemoteSlotManager] Invalid IP, skipping Firebase\n");
            return;
        }

        auto cb = [this](const std::string& msg) { NotifyStatus(msg); };
        if (m_firebase.Initialize(cb))
        {
            m_firebase.CleanupStaleHosts();
            UpdateAvailableSlots();
            m_firebase.StartHeartbeat(m_hostId);
        }
        else
        {
            printf("[RemoteSlotManager] Firebase init failed\n");
        } })
        .detach();
}

// ---------------------------------------------------------------------------
// 物理コントローラー検出
// ---------------------------------------------------------------------------
void RemoteSlotManager::DetectPhysicalControllers()
{
    int physicalCount = 0;
    for (int i = 0; i <= 3; i++)
    {
        if (IsXInputConnected(i))
            physicalCount++;
        else
            break;
    }

    for (int slot = 1; slot <= 4; slot++)
    {
        if (slot <= physicalCount)
        {
            m_slots[slot].isPhysical = true;
            m_slots[slot].mode = SlotMode::LOCAL;
            m_slots[slot].isConnected = false;
            m_slots[slot].label = "Local Controller #" + std::to_string(slot);
        }
        else
        {
            m_slots[slot].isPhysical = false;
            m_slots[slot].mode = SlotMode::LOCAL;
            m_slots[slot].isConnected = false;
            m_slots[slot].label = "";
        }
        NotifySlotChanged(slot);
    }

    printf("[RemoteSlotManager] Local Controller %d found\n", physicalCount);
}

// ---------------------------------------------------------------------------
// スロットモード切り替え
// ---------------------------------------------------------------------------
bool RemoteSlotManager::ToggleSlotMode(int slot)
{
    if (!IsValidSlot(slot))
        return false;
    if (m_slots[slot].isPhysical)
        return false; // 物理コントローラーは変更不可

    if (m_slots[slot].mode == SlotMode::LOCAL)
        return SetRemote(slot);
    else
        return SetLocal(slot);
}

bool RemoteSlotManager::SetRemote(int slot)
{
    if (!IsValidSlot(slot) || m_slots[slot].isPhysical)
        return false;

    // ViGEm仮想コントローラー追加
    if (m_vigem.IsInitialized())
    {
        if (!m_vigem.AddController(slot))
        {
            NotifyStatus("スロット" + std::to_string(slot) + ": Virtual controller failed to initialize");
            return false;
        }
    }

    // UDP受信開始
    int port = XinputReceiver::SlotToPort(slot);
    auto cb = [this](int s, const XInputPacket &p)
    { OnXInputReceived(s, p); };
    m_xinput.StartListening(slot, port, cb);

    m_slots[slot].mode = SlotMode::REMOTE;
    m_slots[slot].isConnected = false;
    m_slots[slot].label = "Standing by... (port " + std::to_string(port) + ")";

    NotifySlotChanged(slot);
    UpdateAvailableSlots();

    printf("[RemoteSlotManager] Slot %d → REMOTE (port=%d)\n", slot, port);
    return true;
}

bool RemoteSlotManager::SetLocal(int slot)
{
    if (!IsValidSlot(slot))
        return false;

    // UDP受信停止
    m_xinput.StopListening(slot);

    // ViGEm仮想コントローラー削除
    m_vigem.RemoveController(slot);

    m_slots[slot].mode = SlotMode::LOCAL;
    m_slots[slot].isConnected = false;
    m_slots[slot].label = "";

    NotifySlotChanged(slot);
    UpdateAvailableSlots();

    printf("[RemoteSlotManager] Slot %d → LOCAL\n", slot);
    return true;
}

// ---------------------------------------------------------------------------
// XInput受信コールバック（スレッドから呼ばれる）
// ---------------------------------------------------------------------------
void RemoteSlotManager::OnXInputReceived(int slot, const XInputPacket &packet)
{
    if (!IsValidSlot(slot) || !m_vigem.IsInitialized())
        return;

    // XInputPacket → XUSB_REPORT に変換
    XUSB_REPORT report = {};
    report.wButtons = packet.wButtons;
    report.bLeftTrigger = packet.bLeftTrigger;
    report.bRightTrigger = packet.bRightTrigger;
    report.sThumbLX = packet.sThumbLX;
    report.sThumbLY = packet.sThumbLY;
    report.sThumbRX = packet.sThumbRX;
    report.sThumbRY = packet.sThumbRY;

    m_vigem.UpdateController(slot, report);
    bool ok = m_vigem.UpdateController(slot, report);
    // printf("[OnXInputReceived] UpdateController result=%d\n", ok);
    //  初回受信時に接続済みに更新
    if (!m_slots[slot].isConnected)
    {
        m_slots[slot].isConnected = true;
        m_slots[slot].label = "connecting (Remote)";
        NotifySlotChanged(slot);
    }
}

// ---------------------------------------------------------------------------
// Firebaseにスロット状態を更新
// ---------------------------------------------------------------------------
void RemoteSlotManager::UpdateAvailableSlots()
{
    if (!m_firebase.IsInitialized() || m_externalIp.empty())
        return;

    bool available[5] = {false};
    available[m_linkplay] = true; // linkplayスロットだけtrue

    // キー=サニタイズ済みIP、PUT/PATCHはlinkplay番号に基づく
    m_firebase.RegisterHost(m_externalIp, available, m_linkplay, m_gameTitle);
}

// ---------------------------------------------------------------------------
// ユーティリティ
// ---------------------------------------------------------------------------
const SlotState &RemoteSlotManager::GetSlotState(int slot) const
{
    static SlotState empty;
    if (!IsValidSlot(slot))
        return empty;
    return m_slots[slot];
}

void RemoteSlotManager::NotifySlotChanged(int slot)
{
    if (m_slotCb && IsValidSlot(slot))
        m_slotCb(slot, m_slots[slot]);
}

void RemoteSlotManager::NotifyStatus(const std::string &msg)
{
    printf("[RemoteSlotManager] %s\n", msg.c_str());
    if (m_statusCb)
        m_statusCb(msg);
}

std::string RemoteSlotManager::GenerateHostId()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    const char hex[] = "0123456789abcdef";
    std::string id(8, '0');
    for (char &c : id)
        c = hex[dis(gen)];
    return id;
}

void RemoteSlotManager::SetSlotOccupied()
{
    if (!m_firebase.IsInitialized() || m_hostId.empty())
        return;
    std::thread([this]()
                { m_firebase.PatchSlotAvailable(m_hostId, m_linkplay, false); })
        .detach();
    printf("[RemoteSlotManager] SetSlotOccupied: slot%d=false\n", m_linkplay);
}

void RemoteSlotManager::SetSlotAvailable()
{
    if (!m_firebase.IsInitialized() || m_hostId.empty())
        return;
    std::thread([this]()
                { m_firebase.PatchSlotAvailable(m_hostId, m_linkplay, true); })
        .detach();
    printf("[RemoteSlotManager] SetSlotAvailable: slot%d=true\n", m_linkplay);
}
#endif // SUPERMODEL_WIN32

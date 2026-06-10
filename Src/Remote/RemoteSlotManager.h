/**
 * RemoteSlotManager.h
 *
 * P1〜P4スロットの状態管理（ViGEm・UDP受信・Firebaseを統括する）
 *
 * VB.NET の Form1.vb の中核ロジックに相当する
 */
#pragma once

#ifdef SUPERMODEL_WIN32

#include <windows.h>
#include <array>
#include <string>
#include <functional>
#include "ViGEmManager.h"
#include "XinputReceiver.h"
#include "FirebaseMatchingCpp.h"
#include "UPnPHelper.h"

// スロットモード
enum class SlotMode
{
    LOCAL, // 物理コントローラー（またはエミュレーター直接入力）
    REMOTE // UDP経由でリモートXInputを受信
};

// スロット状態
struct SlotState
{
    SlotMode mode = SlotMode::LOCAL;
    bool isPhysical = false;  // 物理コントローラーが差さっている
    bool isConnected = false; // リモート接続中（UDP受信中）
    std::string label;        // 表示ラベル
};

using SlotChangedCallback = std::function<void(int slot, const SlotState &state)>;
using StatusCallback = std::function<void(const std::string &message)>;

class RemoteSlotManager
{
public:
    RemoteSlotManager();
    ~RemoteSlotManager();

    // 初期化（ViGEm接続・物理コントローラー検出・Firebase初期化）
    bool Initialize(SlotChangedCallback slotCb = nullptr,
                    StatusCallback statusCb = nullptr);
    void Shutdown();

    // スロットのモードを切り替え（LOCAL ⇔ REMOTE）
    bool ToggleSlotMode(int slot);

    // スロットをリモートモードに設定してUDP受信開始
    bool SetRemote(int slot);
    // スロットをローカルモードに設定してUDP受信停止
    bool SetLocal(int slot);

    // スロット状態を取得
    const SlotState &GetSlotState(int slot) const;

    // ホスト情報
    const std::string &GetHostId() const { return m_hostId; }
    const std::string &GetExternalIp() const { return m_externalIp; }

    // ViGEm・Firebase の状態
    bool IsViGEmReady() const { return m_vigem.IsInitialized(); }
    bool IsFirebaseReady() const { return m_firebase.IsInitialized(); }

    // --- 単独 ViGEm 操作（GUI連動・起動時使用）---
    // ViGEmのみ初期化する（XInput受信は開始しない）
    bool InitViGEm();
    // スロット1に仮想コントローラーを1個作成
    bool AddVirtualController();
    // スロット1の仮想コントローラーを削除
    void RemoveVirtualController();
    // スロット1にUDP受信を開始する（port = 5000 + (linkplay-1)*4）
    bool StartListening(int linkplay);
    // FirebaseスレッドをLaunch時に非同期で起動する
    // gameTitle: ROM名（例: spikeofe）
    void StartFirebaseAsync(const std::string &gameTitle = "");

    // 物理XInputコントローラーの接続確認（xinput1_4.dll使用）
    static bool IsXInputConnected(int userIndex); // userIndex: 0〜3

    void SetSlotOccupied();
    void SetSlotAvailable();

    

private:
    void DetectPhysicalControllers();
    void OnXInputReceived(int slot, const XInputPacket &packet, const std::string &fromIP, int fromPort);
    void UpdateAvailableSlots();
    std::string GenerateHostId();

    ViGEmManager m_vigem;
    XinputReceiver m_xinput;
    FirebaseMatchingCpp m_firebase;

    std::array<SlotState, 5> m_slots; // インデックス 1〜4 を使用
    std::string m_hostId;
    std::string m_externalIp;

    SlotChangedCallback m_slotCb;
    StatusCallback m_statusCb;

    // 仮想コントローラーが作成済みかどうか（スロット1固定）
    bool m_virtualControllerAdded = false;
    // INIから読み込んだlinkplay番号（1〜4）
    int m_linkplay = 0;
    // 起動中のROM名（例: spikeofe）
    std::string m_gameTitle;

    bool IsValidSlot(int slot) const { return slot >= 1 && slot <= 4; }
    void NotifySlotChanged(int slot);
    void NotifyStatus(const std::string &msg);

    bool m_shutdownCalled = false;
};

#endif // SUPERMODEL_WIN32

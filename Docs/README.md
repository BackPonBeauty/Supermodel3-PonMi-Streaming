# Supermodel3-PonMi-Streaming

Sega Model 3エミュレータ [Supermodel3](https://www.supermodel3.com) をベースにした、**WANリモートプレイ対応ストリーミングフォーク**です。

最大4人のプレイヤーがインターネット越しにリンクプレイを楽しめます。

---

## 特徴

- **最大4リンク対応**（ローカルとWAN越しで最大4人）
- **低遅延映像ストリーミング**（NVENC H.264エンコード）
- **音声ストリーミング**（Opus 128kbps）
- **XInputコントローラー入力のUDP転送**
- **Firebaseによるマッチメイキング**（ホスト一覧の自動取得）
- **UPnP自動ポート開放**対応

---

## 動作確認済みタイトル

- Spikeout Final Edition (spikeofe)
<img width="962" height="572" alt="image" src="https://github.com/user-attachments/assets/687436f5-0c9f-4ae9-8fb1-97d1a261725d" />
<img width="962" height="572" alt="image" src="https://github.com/user-attachments/assets/decdc046-8931-4693-8cb5-cee51f7e4f0e" />

---

## 関連リポジトリ

このプロジェクトを使用するには以下のアプリケーションが必要です。

| アプリ | 説明 | リンク |
|--------|------|--------|
| **StreamReceiver** | クライアント側アプリ（映像受信・コントローラー送信） | [BackPonBeauty/StreamReceiver](https://github.com/BackPonBeauty/StreamReceiver)（現在非公開） |
| **XinputReciever (ViGEmReceiver)** | ホスト側起動・管理アプリ | [BackPonBeauty/XinputReciever](https://github.com/BackPonBeauty/XinputReciever)（現在非公開） |

> リリースにはffmpegバイナリが同梱されています。
> ffmpegはLGPL/GPLライセンスです。ソースコードは https://ffmpeg.org で入手できます。

---
<img width="398" height="490" alt="名称未設定-2" src="https://github.com/user-attachments/assets/c4bba737-7d27-4692-befc-ee7fb3aeabe2" />

## 必要環境

### ホスト側

| 項目 | 要件 |
|------|------|
| OS | Windows 10/11 64bit |
| GPU | NVIDIA RTX 20シリーズ以降（NVENC対応） |
| ドライバー | CUDA 13.0以降対応版 |
| その他 | [ViGEmBus](https://github.com/nefarius/ViGEm.Bus) ドライバー導入済み |
| ルーター | UPnP対応（または手動ポート開放） |
| 回線 | 上り 10Mbps以上推奨 |

### クライアント側

| 項目 | 要件 |
|------|------|
| OS | Windows 10/11 64bit |
| ルーター | UPnP対応（または手動ポート開放） |
| 回線 | 下り 5Mbps以上推奨 |

---

## ポート番号

| スロット | XInput | HS/HB | 映像 | 音声 |
|---------|--------|-------|------|------|
| P1 | 5000 | 5001 | 5002 | 5003 |
| P2 | 5004 | 5005 | 5006 | 5007 |
| P3 | 5008 | 5009 | 5010 | 5011 |
| P4 | 5012 | 5013 | 5014 | 5015 |

---

## セットアップ（ホスト側）

### 1. ViGEmBus のインストール

[ViGEmBus Releases](https://github.com/nefarius/ViGEm.Bus/releases) から最新版をダウンロードしてインストールしてください。

### 2. フォルダ構成

```
XinputReciever/
├── ViGEmReceiver.exe
├── supermodel01/
│   ├── supermodel-ponmi.exe
│   ├── ponmi.ini
│   ├── start.bat
│   ├── start.vbs
│   └── ROMs/ (シンボリックリンク推奨)
├── supermodel02/
│   └── ...
├── supermodel03/
│   └── ...
└── supermodel04/
    └── ...
```

### 3. ViGEmReceiver の使い方

1. `ViGEmReceiver.exe` を起動
2. 外部IPが自動取得されます（必要なら手動修正）
3. 各スロット（P1〜P4）をLocal/Remoteに設定
4. **START** ボタンを押す
5. Supermodelが自動起動し、Firebaseにホスト情報が登録されます
6. 終了時は **STOP** ボタンを押してください

---

## セットアップ（クライアント側）

### StreamReceiver の種類

| 版 | 配布場所 | 機能 |
|----|---------|------|
| Free版 | [GitHub Releases](https://github.com/BackPonBeauty/Supermodel3-PonMi-Streaming/releases) | 1600×900固定 |
| Patron版 | [Patreon](https://www.patreon.com/) Bronze以上 | フルスクリーン対応 |

### 接続方法

<img width="494" height="512" alt="名称未設定-1" src="https://github.com/user-attachments/assets/78289230-71a1-4bd5-bd12-f4cc486fb9ee" />


1. `StreamReceiver.exe` を起動
2. ホスト一覧から接続先を選択
3. 空きスロット（緑●）を選んで **接続** ボタンを押す
4. 映像が表示されたらプレイ開始

**キー操作：**
- `Escape` : 切断してホスト選択画面に戻る
- `F11` : フルスクリーン切り替え（Patron版）

---

## ライセンス

このソフトウェアはGPLライセンスの下で配布されます。

以下のライブラリを使用しています：

- **Supermodel3** - GPL / https://www.supermodel3.com
- **ffmpeg** - LGPL/GPL / https://ffmpeg.org
- **Nefarius.ViGEm.Client** - MIT / https://github.com/nefarius/ViGEm.Client
- **Mono.NAT** - MIT / https://github.com/lontivero/Open.NAT
- **Firebase.Database.NET** - MIT / https://github.com/step-up-labs/firebase-database-dotnet

---

## 作者

**背中ポン美 (BackPonBeauty)**

- YouTube: [@BackPonBeauty](https://www.youtube.com/@BackPonBeauty)
- GitHub: [BackPonBeauty](https://github.com/BackPonBeauty)
- X: [@BackPonBeauty](https://x.com/BackPonBeauty)

# Supermodel3-PonMi-Streaming

A **WAN remote play streaming fork** of the Sega Model 3 emulator [Supermodel3](https://www.supermodel3.com).

Up to 4 players can enjoy link play over the internet.

---

## Features

- **Up to 4-link support** (mix of local and remote players over WAN, up to 4 players)
- **Low-latency video streaming** (NVENC H.264 encoding)
- **Audio streaming** (Opus 128kbps)
- **XInput controller input via UDP**
- **Firebase-based matchmaking** (automatic host discovery)
- **UPnP automatic port forwarding** support

---

## Confirmed Working Titles

- Spikeout Final Edition (spikeofe)

---

## Related Repositories

The following applications are required to use this project.

| App | Description | Link |
|-----|-------------|------|
| **StreamReceiver** | Client application (video reception & controller input) | [BackPonBeauty/StreamReceiver](https://github.com/BackPonBeauty/StreamReceiver) (currently private) |
| **XinputReciever (ViGEmReceiver)** | Host management application | [BackPonBeauty/XinputReciever](https://github.com/BackPonBeauty/XinputReciever) (currently private) |

> Releases include **ffmpeg** binaries.
> ffmpeg is licensed under LGPL/GPL. Source code is available at https://ffmpeg.org

---

## Requirements

### Host

| Item | Requirement |
|------|-------------|
| OS | Windows 10/11 64-bit |
| GPU | NVIDIA RTX 20 series or later (NVENC required) |
| Driver | CUDA 13.0 compatible or later |
| Other | [ViGEmBus](https://github.com/nefarius/ViGEm.Bus) driver installed |
| Router | UPnP enabled (or manual port forwarding) |
| Network | 10 Mbps upload or faster recommended |

### Client

| Item | Requirement |
|------|-------------|
| OS | Windows 10/11 64-bit |
| Router | UPnP enabled (or manual port forwarding) |
| Network | 5 Mbps download or faster recommended |

---

## Port Numbers

| Slot | XInput | HS/HB | Video | Audio |
|------|--------|-------|-------|-------|
| P1 | 5000 | 5001 | 5002 | 5003 |
| P2 | 5004 | 5005 | 5006 | 5007 |
| P3 | 5008 | 5009 | 5010 | 5011 |
| P4 | 5012 | 5013 | 5014 | 5015 |

---

## Setup (Host)

### 1. Install ViGEmBus

Download and install the latest release from [ViGEmBus Releases](https://github.com/nefarius/ViGEm.Bus/releases).

### 2. Folder Structure

```
XinputReciever/
├── ViGEmReceiver.exe
├── supermodel01/
│   ├── supermodel-ponmi.exe
│   ├── ponmi.ini
│   ├── start.bat
│   ├── start.vbs
│   └── ROMs/ (symlink recommended)
├── supermodel02/
│   └── ...
├── supermodel03/
│   └── ...
└── supermodel04/
    └── ...
```

### 3. Using ViGEmReceiver

1. Launch `ViGEmReceiver.exe`
2. Your external IP is automatically detected (edit manually if needed)
3. Set each slot (P1–P4) to Local or Remote
4. Press the **START** button
5. Supermodel instances launch automatically and host info is registered to Firebase
6. Press **STOP** to shut down

---

## Setup (Client)

### StreamReceiver Editions

| Edition | Distribution | Features |
|---------|-------------|----------|
| Free | [GitHub Releases](https://github.com/BackPonBeauty/Supermodel3-PonMi-Streaming/releases) | 1600×900 fixed window |
| Patron | [Patreon](https://www.patreon.com/) Bronze or above | Full-screen support |

### How to Connect

1. Launch `StreamReceiver.exe`
2. Select a host from the list
3. Choose an available slot (green ●) and press **Connect**
4. Play begins when video appears

**Key Controls:**
- `Escape` : Disconnect and return to host selection
- `F11` : Toggle full-screen (Patron edition only)

---

## License

This software is distributed under the GPL license.

The following libraries are used:

- **Supermodel3** - GPL / https://www.supermodel3.com
- **ffmpeg** - LGPL/GPL / https://ffmpeg.org
- **Nefarius.ViGEm.Client** - MIT / https://github.com/nefarius/ViGEm.Client
- **Mono.NAT** - MIT / https://github.com/lontivero/Open.NAT
- **Firebase.Database.NET** - MIT / https://github.com/step-up-labs/firebase-database-dotnet

---

## Author

**背中ポン美 (BackPonBeauty)**

- YouTube: [@BackPonBeauty](https://www.youtube.com/@BackPonBeauty)
- GitHub: [BackPonBeauty](https://github.com/BackPonBeauty)
- X: [@BackPonBeauty](https://x.com/BackPonBeauty)

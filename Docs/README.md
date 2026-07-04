# 🎮 Supermodel3-PonMi-Streaming v1.0.2 - Built-in WAN Streaming

**No extra tools required. Just launch, share your slot, and play.**

---
## 🆕 What's New in v1.0.2

---
### 1. XInput Packet Extension — Send Key Input from Client
- **bit 0** = Alt+D (toggle debug panel)
- **bit 1** = Cursor Up (increase bitrate)
- **bit 2** = Cursor Down (decrease bitrate)
- Rising-edge detection prevents repeated triggering
---
### 2. Manual Bitrate Control
- Cursor Up/Down changes the ceiling in 0.5Mbps steps (0.5–3.0Mbps)
- Manual control only adjusts the "ceiling" — adaptive bitrate still follows automatically
- Ceiling resets to startup value (2.5Mbps) when there are 0 clients
---
### 3. Adaptive Bitrate Cleanup
| Item | Before | After |
|------|--------|--------|
| MIN_MAX | 0.5Mbps | 1.0Mbps |
| BASE_MAX formula | avg × 2 | avg + 0.5Mbps |
| Initial avg | 1.5Mbps | **2.5Mbps** (for H.264) |
| Initial peak | 2.0Mbps | **3.0Mbps** |
| vbvInitialDelay | 300000 | **0** |
| AmfEncoder Init | 3.0/4.0Mbps | Unified to **2.5/3.0Mbps** |
---
### 4. Automatic Codec Negotiation
- Added supported codec list to the HELLO packet
  - `HELLO:<nick>:H265,H264` or `HELLO:<nick>:H264`
- Host selects a matching codec and responds with `OK`
- Clients without H.265 support automatically fall back to H.264
---
### 5. Debug Panel Improvements
- Now displays Player / Spectator nicknames
  - Connected: green (Player) / yellow (Spectator)
  - Not connected: gray `---`
---
### 6. MultiView Protection
- MultiView ON requests are ignored if no other Instance exist in shared memory

---


## 🆕 What's New in v1.0.1

- **Streaming built directly into the emulator** — XinputReciever is no longer required
- **Firebase automatic matchmaking** — hosts are discovered automatically, no manual IP entry needed
- **UPnP automatic port forwarding** — no router configuration required in most cases
- **Ping display** — latency to each host is shown in the host list
- **NVENC H.264 / H.265 low-latency streaming** — ~5.8ms encode latency on RTX 20 series or later
- **Up to 4-player WAN link play** (mix of local and remote players supported)

---

## ✅ Confirmed Working Titles

- **Spikeout Final Edition** (spikeofe) — 4-link play confirmed
- Virtua Fighter 3tb (single instance)

---

## ⚙️ Specifications & Limitations

- **XInput only** — titles requiring mouse or light gun input are not supported
- **Up to 3 connections per instance** — one player + up to 2 spectators, or 3 spectators
- If the player disconnects, the next spectator in line becomes the player
- **Auto-disconnect** — clients with no XInput input for 1 minute will be disconnected

---

## 📦 Package Contents

This package is configured for **Spikeout Final Edition (spikeofe) 4-link play**.

```
Spikeofe_4links_Sample.zip
├── 01/
│   ├── supermodel.exe
│   └── Config/
│       └── Supermodel.ini   ← Slot P1
├── 02/                       ← Slot P2
├── 03/                       ← Slot P3
├── 04/                       ← Slot P4
├── ROMs/                     ← Place your ROM files here
├── PonLuncher.exe            ← Launch all 4 instances / manage host 
└── 4lnkstart with cmd.bat    ← Launch all 4 instances (with console window)

```

### Streaming Configuration Patterns

**All remote** (host does not play)

| Slot | Streaming |
|------|-----------|
| 01–04 | `Streaming = 1` |

> StreamReceiver is not required on the host side.

**P1 local + P2–P4 remote** (host plays as P1)

| Slot | Streaming |
|------|-----------|
| 01 | `Streaming = 0` |
| 02–04 | `Streaming = 1` |

> ⚠️ Connect your XInput controller and make sure Windows recognizes it before launching.

> ⚠️ ROM files are not included. Please obtain them legally on your own.

---

## 💻 Requirements

### Host

| Item | Requirement |
|------|-------------|
| OS | Windows 10/11 64-bit |
| GPU | NVIDIA RTX 20 series or later (NVENC required) |
| Driver | CUDA 13.0 compatible or later |
| Other | ViGEmBus |
| Router | UPnP enabled (or manual port forwarding) |
| Network | 20 Mbps upload or faster recommended |

- [ViGEmBus](https://github.com/nefarius/ViGEm.Bus/releases) — Virtual gamepad driver

> Tested on: Core i7-13700F / RTX 4070 Super 12GB / 64GB RAM / 10GbE / Windows 11  
> Minimum recommended: Core i5 9th gen or later

---

## 🔌 Port Numbers(default)

| Slot | XInput | HS/HB | Video | Audio |
|------|--------|-------|-------|-------|
| P1 | 55000 | 55001 | 55002 | 55003 |
| P2 | 55004 | 55005 | 55006 | 55007 |
| P3 | 55008 | 55009 | 55010 | 55011 |
| P4 | 55012 | 55013 | 55014 | 55015 |

---

## 🚀 Setup (Host)

### 1. Install ViGEmBus

Download and install the latest release from [ViGEmBus Releases](https://github.com/nefarius/ViGEm.Bus/releases).

### 2. Allow through Windows Firewall

Allow `supermodel.exe` through Windows Firewall for both private and public networks.

### 3. Configure Supermodel.ini

Set the following in each slot's `Config/Supermodel.ini`:

```ini
Streaming = 1
LinkPlay = 1   ; your slot number (1–4)
```

#### LinkPlay values

| Value | Description | Virtual controllers created |
|-------|-------------|----------------------------|
| `0` | Single title — streams both P1 and P2 from one instance | 2 (P1 + P2) |
| `1` | Link play slot 1 (P1) / Single title: P1 local + P2 streaming | 1 |
| `2` | Link play slot 2 (P2) | 1 |
| `3` | Link play slot 3 (P3) | 1 |
| `4` | Link play slot 4 (P4) | 1 |

> For single-title streaming (e.g. a 2-player cabinet), use `LinkPlay = 0`. Two virtual controllers will be created on the client side.

> For single-title with P1 played locally on the host, use `LinkPlay = 1` and `Streaming = 1`. Connect your XInput controller before launching.

### 4. Place your ROM

Place your ROM files in the **`ROMs/`** folder at the root of the package. The sample `Supermodel.ini` in each slot is already configured to point to this directory.

### 5. Launch supermodel.exe

Run **`4lnkstart.bat`** to launch all 4 instances at once. This is the recommended way to start.

> `4lnkstart with cmd.bat` opens a console window for debugging purposes.

---

> Streaming instances can be minimized — audio can also be muted in Windows mixer. This will not affect streaming performance.

Clients use [StreamReceiver](https://github.com/BackPonBeauty/StreamReceiver) to join.

---

## 🔧 Configuration

### Video Codec (`Supermodel.ini`)

You can select the video codec used for streaming by editing `Config/Supermodel.ini`:

```ini
Decoder = H265   ; Use H.265 (HEVC) — better quality at lower bitrate
Decoder = H264   ; Use H.264 (AVC)  — wider compatibility (default)
```

| Codec | Key | Notes |
|-------|-----|-------|
| H.264 (AVC) | `H264` | Default. Broadest compatibility. |
| H.265 (HEVC) | `H265` | Lower bitrate at equivalent quality. Requires NVENC-capable GPU on host. |

> **Note:** The key name is `Decoder` in the config file, but it should have been `Codec`. This is a known typo.

> Both host and client must support the selected codec.  
> H.265 requires an NVIDIA RTX 20 series or later GPU on the host side.

### H.264 vs H.265 Requirements

| Role | H.264 (default) | H.265 (HEVC) |
|------|-----------------|--------------|
| **Host GPU** | GTX 600 series or later (Kepler, 2012+) | GTX 960 / 950 or later, GTX 10 series or later (Maxwell 2nd gen, 2015/2016+) |
| **Client GPU/CPU** | Compatible with virtually any PC (even ~2011 hardware) | Intel 6th gen CPU or later, or GTX 960 / GTX 10 series or later |

H.264's strength is that it works reliably on older, lower-spec hardware. With H.265, any standard PC released within the last ~10 years can enjoy higher quality streaming at lower bandwidth.

> ⚠️ *Requirements above are based on AI-generated information and may not be fully accurate. Please verify against official NVIDIA/Intel documentation if needed.*

---

## 🖥️ Client App

- [StreamReceiver](https://github.com/BackPonBeauty/StreamReceiver) — Client app (video reception & controller input)

---

## 🔧 Related Repositories

- [StreamReceiver](https://github.com/BackPonBeauty/StreamReceiver) — Client app (video reception & controller input)
- [Supermodel3-PonMi](https://github.com/BackPonBeauty/Supermodel3-PonMi) — Base PonMi edition emulator

---

## ⚠️ Windows SmartScreen

A SmartScreen warning may appear on first launch due to the absence of a code signing certificate.  
Click **"More info" → "Run anyway"** to proceed.

---

## 📜 License

This project is released under the **GPL v3** license.  
Based on the original [Supermodel3](https://www.supermodel3.com).

### Third-party Libraries

- ffmpeg `ffmpeg-2026-06-01-git-bf608f16fd-essentials_build` — [LGPL 2.1 / GPL 2.0](https://ffmpeg.org) / Build by [gyan.dev](https://www.gyan.dev/ffmpeg/builds/)
- NVENC / CUDA — NVIDIA License
- [NVIDIA Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk-license-agreement) — NVIDIA Toolkit License
- miniupnpc — BSD License
- ViGEm — BSD License
- Firebase C++ SDK — Apache 2.0

---

## 💬 Community

- [BackPonBeauty](https://github.com/BackPonBeauty) — GitHub
- [patreon.com/PonMi](https://patreon.com/PonMi) — Patreon
- [discord.gg/mNjPJHTTen](https://discord.gg/mNjPJHTTen) — Discord
- [back_pon_beauty](https://www.youtube.com/@backponbeauty) — YouTube
- [back_pon_beauty](https://twitch.tv/back_pon_beauty) — Twitch

---
---


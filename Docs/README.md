# Supermodel3-PonMi-Streaming

A **built-in WAN streaming fork** of the Sega Model 3 emulator [Supermodel3](https://www.supermodel3.com).

Up to 4 players can enjoy link play over the internet — **no extra tools required**.

---

## What's New in v2.0.0

- **XinputReciever is no longer required** — streaming is now built directly into the emulator
- **Firebase-based automatic matchmaking** — hosts are discovered automatically, no manual IP entry needed
- **UPnP automatic port forwarding** — no router configuration required in most cases
- **Ping display** — latency to each host is shown in the host list

---

## Features

- **Up to 4-player WAN link play** (mix of local and remote players)
- **Low-latency video streaming** (NVENC H.264 encoding)
- **Audio streaming** (Opus 128kbps)
- **XInput controller input via UDP**
- **Firebase-based matchmaking** (automatic host discovery)
- **UPnP automatic port forwarding**
- **Ping measurement** per host

---

## Confirmed Working Titles

<img width="962" height="572" alt="image" src="https://github.com/user-attachments/assets/687436f5-0c9f-4ae9-8fb1-97d1a261725d" />
<img width="962" height="572" alt="image" src="https://github.com/user-attachments/assets/decdc046-8931-4693-8cb5-cee51f7e4f0e" />

- Spikeout Final Edition (spikeofe)

---

## Related Repositories

| App | Description | Link |
|-----|-------------|------|
| **StreamReceiver** | Client application (video reception & controller input) | [BackPonBeauty/StreamReceiver](https://github.com/BackPonBeauty/StreamReceiver) |

> XinputReciever (ViGEmReceiver) is **no longer required** as of v2.0.0.

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

### 2. Configure Supermodel.ini

Set the following in `Supermodel.ini`:

```ini
Streaming = 1
LinkPlay = 1   ; 1-4 (your player slot)
```

### 3. Launch

Just launch `supermodel-ponmi.exe` with your ROM.  
Your host is automatically registered to Firebase and becomes discoverable by clients.

---

## Setup (Client)

### How to Connect

<img width="962" height="572" alt="名称未設定-1" src="https://github.com/user-attachments/assets/1838e45e-f115-4baf-b011-d81bfd997b48" />

1. Launch `StreamReceiver.exe`
2. Select a host from the list (ping is displayed next to each host)
3. Choose an available slot (●) and press **Connect**
4. Play begins when video appears

**Key Controls:**
- `Escape` : Disconnect and return to host selection
- `F11` : Toggle full-screen (Patron edition only)

---

## License

This software is distributed under the GPL v3 license.

The following libraries are used:

- **Supermodel3** - GPL / https://www.supermodel3.com
- **ffmpeg (essentials build by gyan.dev)** - LGPL/GPL / https://ffmpeg.org | https://www.gyan.dev/ffmpeg/builds/
- **Nefarius.ViGEm.Client** - MIT / https://github.com/nefarius/ViGEm.Client
- **Mono.NAT** - MIT / https://github.com/lontivero/Open.NAT
- **Firebase.Database.NET** - MIT / https://github.com/step-up-labs/firebase-database-dotnet
- **miniupnpc** - BSD / https://miniupnp.tuxfamily.org
- **NVIDIA Video Codec SDK (NVENC)** - NVIDIA Software License / https://developer.nvidia.com/nvidia-video-codec-sdk
- **CUDA Toolkit** - NVIDIA Software License / https://developer.nvidia.com/cuda-toolkit
- **Opus** - BSD / https://opus-codec.org

---

## Author

**背中ポン美 (BackPonBeauty)**

- YouTube: [@BackPonBeauty](https://www.youtube.com/@BackPonBeauty)
- GitHub: [BackPonBeauty](https://github.com/BackPonBeauty)
- X: [@BackPonBeauty](https://x.com/BackPonBeauty)
- Discord: [discord.gg/mNjPJHTTen](https://discord.gg/mNjPJHTTen)
- Patreon: [patreon.com/BackPonBeauty](https://patreon.com/Ponmi)

# ZeroPoint v4.1

## Architecture
**ZeroPoint** is a premium, stealth-oriented Windows utility that provides AI assistance over proctoring environments. v4.1 extends the core stealth engine, adding a fully isolated **Virtual Environment** running on loopback RDP, integrated directly into a DWM-frosted Windows host overlay.

## Features

### Stealth Payload Injection
ZeroPoint performs Process Hollowing into `svchost.exe` via `PerformHollowing()`. The payload is written with `WriteProcessMemory` and validated with `FlushInstructionCache` before `ResumeThread`. The UI process runs on the host with `WDA_EXCLUDEFROMCAPTURE` applied to every window, hiding it completely from recording APIs.

### Premium Launcher & Settings
Using raw GDI+ and DWM Blur Behind (`DwmEnableBlurBehindWindow`), ZeroPoint presents a pristine, icy `[#00DDFF]` cyan and white frosted glass design.
The standalone **ZeroPoint Launcher** (440x400) provides one-click access:
1. Start Virtual Environment
2. Open Settings Gear -> `Interception`, `Display`, and `Local Resources`.

### Complete Proctor Evasion
The host application uses standard Windows Defender exclusions configured during the Inno Setup installer. The "Proctor Coverage" interface natively targets:
* Respondus LockDown Browser
* Guardian Browser
* Safe Exam Browser
* QuestionMark Secure
* PearsonVue OnVue

### Loopback RDP Virtual Environment (VE)
By dynamically pulling and mutating an `rdpwrap.ini` for concurrent headless Terminal Services, ZeroPoint provides a standalone, fully hardware-accelerated Windows desktop. The VE captures system input safely and protects the screen contents via the `CLICK TO LOCK` system.

### Hardware Spoofing (Admin Required)
On VE startup, `ApplyHardwareSpoofing()` randomizes BIOS/SMBIOS vendor, CPU ID, GPU driver description, disk serial, and MAC address via HKLM registry writes. Uses a pooled cryptographic RNG (`CryptGenRandom` with 256-byte buffer) for high-quality randomness without per-call overhead. MAC addresses use the locally-administered bit (02:xx) for protocol compliance. Requires elevated (Run as Administrator) privileges; gracefully warns if writes fail.

### AI Engine Modes & Providers
ZeroPoint queries multi-provider Chat Completions and Vision models via direct API calls without telemetry.
**Available Providers**:
* **Claude 4.6 Opus** (api.anthropic.com)
* **GPT-5.2** (api.openai.com)
* **Grok 4** (api.x.ai)
* **Deepseek V3.2 R1** (api.deepseek.com)
* **OpenRouter** (openrouter.ai)

**Modes**:
1. **Auto-Send Mode**: Instantly captures the screenshot and fires a multi-modal payload. Automatically detects if vision is available and falls back to CDP DOM extraction.
2. **Add-to-Chat Mode**: Stores the thumbnail into the side-panel scratchpad for native drag-and-drop upload.

### Auto-Typer
Human-like text injection via `PerformAutoType()`. Uses gamma-distributed keystroke timing (sum of 3 uniform samples via pooled `CryptoRandByte`), punctuation pauses, micro-hesitations, rare longer pauses, and occasional typo+backspace simulation. Bias-free random selection via rejection sampling. Statistically indistinguishable from real typing.

## Keybinds

| Keybind | Function |
| :--- | :--- |
| **Ctrl+Shift+Z** | Take Full Screenshot / Add to Scratchpad |
| **Ctrl+Shift+S** | Snip Region (Crosshair to capture custom area) |
| **Ctrl+Shift+T** | Auto-Typer — injects last AI answer with gamma-distributed keystroke timing + typo simulation |
| **Ctrl+Alt+H** | Toggle Full Sidebar (Host) |
| **Ctrl+Alt+C** | Lock/Unlock Virtual Environment (VE Lock with Mouse Teleport) |
| **Ctrl+Alt+F** | Toggle Fullscreen VE |
| **Ctrl+Alt+B** | Toggle Invisible WebView2 Browser |
| **Ctrl+Shift+R** | Rapid Fire Thoughts overlay (Live AI streaming status updates) |
| **Ctrl+Shift+X** | Panic Killswitch (Wipes all traces in `ProgramData`) |

## Build

From a Developer Command Prompt for VS 2022:
```
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Requires: Windows 10 SDK (10.0.19041.0+), WebView2 NuGet package, Edge WebView2 Runtime.

## v4.1 Changelog
- **Performance**: Pooled CryptoRandByte (256-byte buffer) eliminates per-call CryptAcquireContext overhead
- **Performance**: Screenshot history uses `std::deque` for O(1) eviction instead of O(n) vector erase
- **Performance**: Reduced unnecessary Sleep durations throughout (Defender exclusions, sidebar hide, TermService restart)
- **Bug Fix**: GDI handle leak in CaptureScreenshotBitmap (old bitmap not restored before DeleteDC)
- **Bug Fix**: `g_HoveredThumb` used before declaration — moved to correct scope
- **Bug Fix**: ZPDataObject double-free risk — HGLOBAL ownership tracked after DoDragDrop
- **Bug Fix**: BrowserNavigate incorrectly fired on EN_KILLFOCUS (edit losing focus)
- **Bug Fix**: Redundant OleInitialize + CoInitializeEx — removed duplicate COM init
- **Bug Fix**: Missing FlushInstructionCache after WriteProcessMemory in process hollowing
- **Bug Fix**: Missing VirtualFreeEx on failure paths in ghost process injection
- **Bug Fix**: PanicKillAndWipe now stops VE and kills RDP sessions before wiping
- **Bug Fix**: KillAllRDPSessions now only kills our mstsc process (tracked by PID)
- **Bug Fix**: Missing StopVirtualEnvironment on normal exit path
- **Safety**: DownloadFile redirect depth limit (max 5) prevents infinite redirect loops
- **Safety**: HTTP status code 200 validation before writing downloaded files
- **Safety**: Config save is atomic (write to .tmp, then rename)
- **Safety**: Bounds-checked config parsing with SafeAtoi
- **Stealth**: Expanded hardware spoofing pools (7 board manufacturers, 5 CPUs, 5 GPUs)
- **Stealth**: MAC address spoofing uses locally-administered bit (02:xx)
- **Stealth**: Bias-free random selection via rejection sampling (CryptoRandUniform)
- **Stealth**: Auto-typer adds rare longer pauses (~0.5%) simulating brief distraction
- **64-bit**: GetWindowLongPtr/SetWindowLongPtr for correct 64-bit mstsc embedding
- **Build**: Application manifest embeds DPI awareness (PerMonitorV2), admin elevation, UTF-8 code page
- **Build**: MSVC optimizations (/O2, /GL, /LTCG) and multi-process compilation (/MP)

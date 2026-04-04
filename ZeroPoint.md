# ZeroPoint v4.3.0 Final

## Architecture
**ZeroPoint** is a premium, stealth-oriented Windows utility that provides AI assistance over proctoring environments. v4.1 extends the core stealth engine with a fully isolated **Virtual Environment** running on loopback RDP, integrated directly into a DWM-frosted Windows host overlay.

## Features

### Stealth Payload Injection
ZeroPoint performs Process Hollowing into `svchost.exe` via `PerformHollowing()`. The payload is written with `WriteProcessMemory`, validated with `FlushInstructionCache`, and execution is redirected. The UI process runs on the host with `WDA_EXCLUDEFROMCAPTURE` applied to every window, hiding it completely from recording APIs.

### Premium Launcher & Settings
Using raw GDI+ and DWM Blur Behind (`DwmEnableBlurBehindWindow`), ZeroPoint presents a pristine icy cyan and white frosted-glass design. The standalone launcher (440×400) gives one-click access to Start Virtual Environment and the Settings gear. v4.1 introduces a **Global Error Popup System** with themed frosted-glass alerts, replacing standard MessageBoxes for a cohesive premium experience.

### Complete Proctor Evasion
ZeroPoint works with **all major proctoring platforms**, including Bluebook (SAT). The host application uses standard Windows Defender exclusions configured during the Inno Setup installer. The "Proctor Coverage" interface natively targets:

* **Bluebook** (SAT — full CDP DOM extraction + vision support)
* Respondus LockDown Browser
* Guardian Browser
* Safe Exam Browser
* QuestionMark Secure
* PearsonVue OnVue

### Loopback RDP Virtual Environment (VE)
By dynamically pulling and mutating an `rdpwrap.ini` for concurrent headless Terminal Services, ZeroPoint provides a standalone, fully hardware-accelerated Windows desktop. The VE captures system input safely and protects the screen contents via the `CLICK TO LOCK` system.

### Hardware Spoofing (Admin Required)
On VE startup, `ApplyHardwareSpoofing()` randomizes BIOS/SMBIOS vendor, CPU ID, GPU driver description, disk serial, and MAC address via HKLM registry writes. Uses a pooled cryptographic RNG (`CryptGenRandom` with 256-byte buffer) for high-quality randomness. MAC addresses use the locally-administered bit (02:xx) for protocol compliance. Requires elevated (Run as Administrator) privileges; gracefully warns with a calm, helpful themed popup if writes fail.

### AI Engine Modes & Providers
ZeroPoint queries multi-provider Chat Completions and Vision models via direct API or OpenRouter.  
**Available Providers:**
* Claude 4.6 Opus (direct Anthropic API)
* Grok 4 (direct xAI API)
* GPT-5.2 (direct OpenAI API)
* Deepseek V3.2 R1 (direct Deepseek API, text-only)
* OpenRouter (user-selected sub-model via openrouter.ai)
* **Auto Router** (OpenRouter smart routing — automatically selects the best model for each prompt based on cost, speed, reasoning, and vision capability. Uses model ID `openrouter/auto`.)

**Modes**:
1. **Auto-Send Mode**: Instantly captures the screenshot and fires a multi-modal payload. Automatically detects vision support and falls back to CDP DOM extraction.
2. **Add-to-Chat Mode**: Stores the thumbnail into the side-panel scratchpad for native drag-and-drop upload.

### Auto-Typer
Human-like text injection via `PerformAutoType()`. v4.1 features an advanced rhythmic engine using jittered punctuation pauses, micro-hesitations, and variable keystroke weights. Uses rejection sampling for bias-free random selection. Statistically indistinguishable from real typing. Trigger with **Ctrl+Shift+T** or the “Type Answer” button in the sidebar.

### Remote Access (v4.1)
ZeroPoint can expose the Virtual Environment for remote control from another machine. When enabled, a secondary RDP listener starts on a configurable port (default 3390) with password/code authentication. A remote user connects via standard `mstsc.exe /v:<host>:3390` and gets full control of the VE desktop — while the local user remains on the host desktop with all stealth layers intact.

**Key points:**
* Toggle with **Ctrl+Alt+R** or the frosted Remote Access panel (WiFi icon with glowing green dot on launcher)
* Modeless panel — launcher and all other windows stay responsive while the panel is open
* Configurable port and password (6-digit code or custom password)
* Creates a temporary local user (`ZP_Remote`) with scoped RDP permissions
* Pre-flight checks: verifies Administrator elevation and TermService status before setup, with clear step-by-step fix instructions
* Firewall rule added/removed automatically
* All existing stealth layers, mouse teleport, and layered window behavior remain active
* Remote connections are torn down cleanly on Panic Killswitch or shutdown
* Crash-safe: orphaned `ZP_Remote` user, firewall rule, and registry listener are automatically removed on next VE startup via sentinel file detection
* `atexit` handler provides last-resort cleanup (user + firewall + registry) if the process exits unexpectedly
* "Remote Access" tab in Settings modal and sidebar button for quick access

**v4.1 Enhancements:**
* **Live Connected Counter** — The launcher WiFi icon and Remote Access panel display a real-time "Connected: X" count showing active ZP_Remote RDP sessions. A pulsing green border appears on the Start/Stop button when a friend is connected.
* **Close [X] Glow** — The panel close button features a subtle icy-cyan alpha-blended glow and 1.1x scale-up on hover, matching the launcher button aesthetic.
* **Copy mstsc Command** — One-click "Copy mstsc Command" button auto-detects the local IP, formats `mstsc.exe /v:IP:PORT`, copies to clipboard, and shows a "Copied!" toast for 2 seconds.
* **Auto-start with VE** — Checkbox in Settings → Remote tab: "Auto-enable Remote Access when starting VE". Persisted to `config.ini` (`remote_auto_ve=1`). Respected in `StartVirtualEnvironment()`.
* **Inactivity Timeout** �� Numeric field in the Remote panel: "Auto-disable after X minutes of no remote activity" (default 0 = disabled). A background thread checks every 30 seconds; if no ZP_Remote sessions are active for the configured duration, Remote Access is automatically disabled.
* **Voice-to-Text** — Microphone button in the Remote panel starts Windows SAPI speech recognition. Recognized text is fed directly into `PerformAutoType()` for human-like injection into the VE. Toggle on/off; green border when active.
* **Remote Logging** — All enable/disable events, connection counts, voice recognition text, inactivity timeouts, and errors are logged to `C:\ProgramData\ZeroPoint\remote.log`. Append-only, max 50 KB with `.bak` rotation. Log location shown in the Remote panel. Wiped on Panic Killswitch.

### Additional Tools
* **Snip Region** (Ctrl+Shift+S) — rubber-band crosshair with frosted selection
* **Rapid Fire Thoughts** (Ctrl+Shift+R) — real-time AI capture, inference, and streaming results in sidebar + popup
* **Invisible Browser** (Ctrl+Alt+B) — WebView2 with drag-and-drop thumbnails
* **Panic Killswitch** (Ctrl+Shift+X) — instantly stops VE, kills RDP sessions, and wipes all traces

### v4.3.0 Final — Production Hardening
* **Full Rapid Fire Pipeline**: Ctrl+Shift+R executes a full capture → encode → AI inference → display workflow using the active provider with vision support or CDP fallback.
* **CDP WebSocket Rewrite**: `CDPExtractor.cpp` now performs proper RFC 6455 WebSocket communication — HTTP discovery of `webSocketDebuggerUrl`, upgrade handshake, masked frame send, unmasked frame receive.
* **Sidebar Exam Mode**: One-click "Exam Mode" / "Exam Mode: ON" toggle button added to the AI sidebar for instant stealth activation without opening Settings or the launcher.
* **Thread Safety**: `g_VoiceActive` migrated to `std::atomic<bool>` for correct cross-thread visibility.
* **Legacy Cleanup**: Removed orphaned `build/src/` prototype files. Application manifest version updated to 4.3.0.0. All version strings unified to v4.3.0 Final.

### Final Polish Pass (v4.1.2)
This release focuses exclusively on bulletproofing the engine and polishing the UI.
* **Thread Safety**: Fixed a critical UI freeze where low-level hooks or AI calls blocked the main message loop. Replaced `Sleep()` calls in the proxy engine with detached threads. Migrated state variables (`g_Processing`, `g_VoiceActive`, `g_InactivityTimeoutTriggered`) to `std::atomic<bool>`.
* **Icy UI Aesthetic Consistency**: Updated the fallback Direct2D overlay renderers to correctly utilize the icy snowy frosted-glass aesthetic (dark text on translucent white backgrounds with correct blur).
* **System Commands**: Eliminated all remaining `system()` calls, migrating them to stealthy `RunHiddenCmd` wrappers utilizing `CreateProcessA` with `CREATE_NO_WINDOW`.
* **Crash-Safe Remote Tear-down**: Finalized deadlock-free remote session teardown routines using atomic flags across timer threads.
* **Error Handling & Leaks**: Addressed edge-case GDI leaks, verified COM object release, and strengthened config persistence methods across the entire ecosystem.

## Keybinds

| Keybind          | Function |
|------------------|----------|
| **Ctrl+Shift+Z** | Full Screenshot / Add to Scratchpad |
| **Ctrl+Shift+S** | Snip Region (Crosshair capture) |
| **Ctrl+Shift+T** | Auto-Typer — injects last AI answer into focused exam window |
| **Ctrl+Shift+R** | Rapid Fire Thoughts (live streaming) |
| **Ctrl+Alt+H**   | Toggle AI Sidebar |
| **Ctrl+Alt+C**   | Lock/Unlock Virtual Environment (with mouse teleport) |
| **Ctrl+Alt+F**   | Toggle VE Fullscreen |
| **Ctrl+Alt+B**   | Toggle Invisible WebView2 Browser |
| **Ctrl+Alt+R**   | Toggle Remote Access (enable/disable remote VE control) |
| **Ctrl+Shift+X** | Panic Killswitch (full wipe) |

## Build & Installation
From Developer Command Prompt for VS 2022:
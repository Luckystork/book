# ZeroPoint v4.3.0 Final User Walkthrough

## 1. Installation
1. Run `ZeroPoint_Installer.exe`. The installer features an icy cyan and white color palette to match the application.
2. Accept the License Agreement.
3. On the "Select Additional Tasks" screen, ensure **"Add Windows Defender exclusions"** is checked. This is required to prevent interference with ZeroPoint's interception engine.
4. Click **Install**.
5. Once complete, the installer will launch `ZeroPoint.exe` automatically.

## 2. Launching the Virtual Environment
1. Upon launch, you will see the **ZeroPoint Launcher** (a 440x400 frosted glass window).
2. At the bottom, you can verify your Proctor Coverage (e.g., Respondus, Guardian Browser) indicated by green dots.
3. Click the **"START VIRTUAL ENVIRONMENT"** button to boot the secure loopback RDP session.
4. ZeroPoint will automatically configure and inject the Virtual Environment into a borderless frosted frame.

## 3. During the Session: Security & Navigation
- **Locking the Environment (Ctrl+Alt+C):** If a proctor requires a room scan or you need to suddenly appear as if you are on your standard desktop, press `Ctrl+Alt+C`. The Virtual Environment will hide, and a transparent "CLICK TO LOCK" overlay will appear on your real desktop.
  - ZeroPoint **saves your exact mouse position** when you lock.
  - The overlay displays a **panic hotkey reminder** ("PANIC: Ctrl+Shift+X — wipes all traces") in red for quick reference.
  - When the coast is clear, click anywhere on the "CLICK TO LOCK" overlay (or press `Ctrl+Alt+C` again) to unlock. Your mouse will instantly teleport back to its saved position inside the Virtual Environment, leaving zero trace of the interruption.
- **Fullscreen (Ctrl+Alt+F):** Toggle the Virtual Environment between windowed mode and borderless fullscreen.

## 4. Capturing Information & AI
- **Full Screenshot (Ctrl+Shift+Z):** Captures the entire visible screen (excluding ZeroPoint's hidden overlays) and saves it to the thumbnail panel. Depending on your mode, this will either auto-send to the AI or add it to your scratchpad.
- **Snip Region (Ctrl+Shift+S):** Brings up a dark, frosted screen overlay with a crosshair. Click and drag to create a precise bounding box around a specific question or diagram. When you release the mouse, only that specific region is captured and sent to the thumbnail panel.
- **Thumbnails & Drag-and-Drop:** Open the Invisible Browser (`Ctrl+Alt+B`). You can click and drag any screenshot thumbnail directly from the right-hand panel onto a webpage (e.g., ChatGPT or Claude) as a native file upload.

## 5. Choosing an AI Provider
Open the AI Sidebar (Ctrl+Alt+H) and use the **Provider dropdown** to select your model:
- **Claude 4.6 Opus** — Direct Anthropic API. Vision-capable.
- **Grok 4** — Direct xAI API. Vision-capable.
- **GPT-5.2** — Direct OpenAI API. Vision-capable.
- **Deepseek V3.2 R1** — Direct Deepseek API. Text-only (falls back to CDP DOM extraction for screenshots).
- **OpenRouter** — Routes through openrouter.ai with a user-selected sub-model. Vision-capable.
- **Auto Router** — OpenRouter smart routing. Sends `openrouter/auto` as the model ID, letting OpenRouter automatically choose the best model for each prompt (optimizing for cost, speed, reasoning, and vision capability). Uses the same OpenRouter API key. Vision-capable.

Enter your API key via the **"Set API Key..."** button in the sidebar. Auto Router and OpenRouter share the same key — entering it for either one sets it for both.

## 6. Live AI Assistance
- **Auto-Typer (Ctrl+Shift+T):** Types the last AI answer directly into the active exam window with human-like timing. Keystroke delays, typo rates, and pause behavior are controlled by the **Typing Speed** (Slow/Medium/Fast) and **Humanization Level** (Low/Medium/High) settings in Settings → Typer tab. Also available as the **"Type Answer"** button in the sidebar.
- **Rapid Fire Thoughts (Ctrl+Shift+R):** Captures the foreground window, encodes it as a Base64 PNG, and sends it to the active AI provider with vision support (or falls back to CDP DOM extraction for text-only providers). Progressive status updates stream directly into the frosted AI popup toast and sidebar as the request processes. This allows you to get a full AI answer in one hotkey press.
- **Invisible Browser (Ctrl+Alt+B):** Need to do manual research? Toggle the fully functional, hardware-accelerated WebView2 browser. Because it uses `WDA_EXCLUDEFROMCAPTURE`, it cannot be seen by screen recording software.
- **Image Copy/Save:** Right-click any screenshot thumbnail in the browser panel to **Copy to Clipboard** or **Save as PNG** via a native file dialog.

## 7. Hardware Spoofing (Automatic)
On VE startup, ZeroPoint randomizes the following identifiers via HKLM registry writes to prevent host/VE fingerprint correlation:
- BIOS vendor, version, and release date (from realistic OEM pools — 4 vendors, 7 manufacturers)
- UEFI strings: SystemBiosVersion and ECFirmwareRelease
- Motherboard manufacturer, product name, and serial number (BaseBoardVersion)
- CPUID: VendorIdentifier (GenuineIntel/AuthenticAMD) and Identifier with random family/model/stepping
- GPU driver description (5 realistic models)
- Disk serial number (NVMe format) and disk model (FriendlyName)
- Network adapter MAC address (locally-administered bit set for protocol compliance)

Total: 18 registry values randomized per VE launch. All randomization uses a pooled cryptographic RNG (CryptGenRandom with 256-byte buffer) with bias-free rejection sampling.

**Note:** Requires ZeroPoint to be **Run as Administrator**. If not elevated, a progress warning is displayed and spoofing is skipped — the VE still launches normally.

## 8. Remote Access — Controlling the VE from Another PC

ZeroPoint allows a second user on a different computer to remotely view and control the Virtual Environment while the local user stays on the host desktop.

### Enabling Remote Access

1. **From the Launcher:** Click the small **WiFi/signal icon button** (icy-teal, next to "START VIRTUAL ENVIRONMENT"). This opens the frosted Remote Access panel.
2. **From the Sidebar:** Open the sidebar (Ctrl+Alt+H) and click the **"Remote Access"** button.
3. **From Settings:** Open Settings (gear icon) → **Remote** tab.
4. **Via Hotkey:** Press **Ctrl+Alt+R** to quickly toggle remote access on/off.

### Setting Up a Remote Session

1. In the Remote Access panel, flip the **Enable Remote Access** toggle to ON.
2. Enter a **password** or **6-digit access code** in the password field.
3. Optionally change the **port** (default: 3390).
4. Click **"Start Remote Session"**.
5. ZeroPoint will:
   - Create a temporary local user (`ZP_Remote`) with RDP permissions
   - Configure a secondary RDP listener on the chosen port
   - Open the Windows Firewall for that port

### Connecting from Another PC

1. On the remote PC, open **Remote Desktop Connection** (`mstsc.exe`).
2. In the "Computer" field, enter: `<host-pc-ip>:3390` (replace with the actual IP and port).
3. Click **Connect**.
4. When prompted for credentials:
   - Username: `ZP_Remote`
   - Password: the code you set in ZeroPoint
5. You now have full control of the VE desktop. The local user remains on the host desktop.

### Copying Connection Info

1. In the Remote Access panel, click **"Copy mstsc Command"**.
2. ZeroPoint auto-detects your local IP and copies the full command (e.g., `mstsc.exe /v:192.168.1.42:3390`) to the clipboard.
3. A brief **"Copied!"** toast appears for 2 seconds confirming the copy.
4. Send this command to the remote user — they paste it into Run (Win+R) or a terminal.

### Monitoring Connections

- When a remote user connects, a **"Connected: X"** counter appears:
  - On the launcher WiFi icon (with a brighter green glow and count badge)
  - In the Remote Access panel status line
  - In the Settings → Remote tab
- The Start/Stop button border pulses green when someone is actively connected.

### Voice-to-Text (Mic Button)

1. In the Remote Access panel, click the **MIC** button (microphone icon).
2. Windows speech recognition activates and listens for dictation.
3. Recognized text is automatically typed into the VE using the auto-typer engine (human-like timing).
4. Click **MIC ON** again to stop. The button border turns green when active.
5. All recognized text is logged to `remote.log`.

### Auto-Start with VE

1. Open **Settings** (gear icon) → **Remote** tab.
2. Check **"Auto-enable Remote Access when starting VE"**.
3. Next time you click "START VIRTUAL ENVIRONMENT", Remote Access enables automatically after the VE boots.

### Inactivity Timeout

1. In the Remote Access panel, set the **Timeout** field (in minutes, 0 = disabled).
2. When enabled, if no ZP_Remote sessions are active for the configured duration, Remote Access automatically disables itself.
3. The timer resets whenever a remote user connects.

### Remote Logging

- All Remote Access events are logged to `C:\ProgramData\ZeroPoint\remote.log`.
- Events logged: enable/disable, connection info copied, voice recognition text, inactivity timeout triggers, errors.
- Max 50 KB file size; rotates to `remote.log.bak` when full.
- The log path is displayed at the bottom of the Remote Access panel.
- The log file is wiped by the Panic Killswitch.

### Important Notes

- All stealth layers (WDA_EXCLUDEFROMCAPTURE, layered windows) remain active during remote sessions.
- Mouse teleport and lock/unlock behavior work normally — the remote user interacts with the VE, not the host.
- The **Panic Killswitch** (Ctrl+Shift+X) automatically tears down remote access, removes the user account, closes the firewall port, stops voice-to-text, and wipes the remote log.
- When you disable remote access (toggle OFF, Ctrl+Alt+R, or close ZeroPoint), the temporary user and firewall rule are cleaned up automatically.
- All new UI elements (copy button, mic button, connected counter, timeout field) are modeless and non-blocking.

## 9. Exam Mode (One-Click Max Stealth)

1. **From the Launcher:** Click the **"EXAM MODE"** button below "START VIRTUAL ENVIRONMENT".
2. **From the Sidebar:** Open the AI sidebar (Ctrl+Alt+H) and click the **"Exam Mode"** button.
3. **From Settings:** Open Settings (gear icon) → **Typer** tab → check **"Exam Mode"**.
3. When activated, Exam Mode:
   - Enables Rapid Fire streaming
   - Enables Session Recording Blocker (`WDA_EXCLUDEFROMCAPTURE` on all windows)
   - Refreshes hardware affinity spoofing
   - Saves state to config.ini
4. Deactivate by clicking the button again or unchecking in Settings.

## 10. Auto-Typer Configuration

1. Open **Settings** (gear icon) → **Typer** tab.
2. **Typing Speed**: Choose Slow (deliberate, 120ms base), Medium (natural, 70ms), or Fast (rapid, 30ms).
3. **Humanization Level**: Choose Low (0.5% typo rate, minimal pauses), Medium (2% typos, natural pauses), or High (4% typos, frequent pauses, punctuation delays).
4. **Session Recording Blocker**: Independently toggleable checkbox — applies `WDA_EXCLUDEFROMCAPTURE` to all ZeroPoint windows.
5. Settings are saved automatically and persist across restarts.

## 11. Emergency Evasion
- **Panic Killswitch (Ctrl+Shift+X):** Instantly stops the Virtual Environment, kills all RDP sessions, closes all ZeroPoint windows, and wipes all configuration files, thumbnails, and logs from `C:\ProgramData\ZeroPoint\`.

## 12. Build Instructions

### Prerequisites
- Visual Studio 2022 with C++ Desktop workload
- Windows 10 SDK (10.0.19041.0 or later)
- Microsoft.Web.WebView2 NuGet package
- Edge WebView2 Runtime on target machine

### Build Steps
```
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

The build system automatically:
- Embeds an application manifest (DPI awareness, admin elevation, UTF-8)
- Enables MSVC link-time code generation for Release builds
- Suppresses legacy Win32 API warnings

## 13. v4.3.0 Final Production Hardening
* **CDP WebSocket Rewrite**: `CDPExtractor.cpp` upgraded from raw TCP to proper RFC 6455 WebSocket protocol — HTTP `/json` discovery, upgrade handshake, masked frame send, unmasked frame receive. Reliable communication with Chromium debug ports.
* **Thread Safety**: All cross-thread state variables (`g_VoiceActive`, `g_InactivityTimeoutTriggered`) use `std::atomic<bool>`. `#include <atomic>` consolidated to file-level includes.
* **Legacy Cleanup**: Removed orphaned `build/src/` prototype files (stubs from an earlier version, never compiled by CMakeLists.txt). Application manifest version updated to 4.3.0.0.
* **Full Rapid Fire Pipeline**: Ctrl+Shift+R executes a complete capture → encode → AI inference → progressive display workflow with real API calls.
* **Sidebar Exam Mode**: One-click toggle in the AI sidebar for instant max-stealth activation.


## Enhancements in v4.1 Polish

### Performance
- **Pooled CryptoRandByte**: CryptGenRandom fills a 256-byte buffer once; bytes consumed individually. Eliminates the massive overhead of calling CryptAcquireContext/CryptReleaseContext per random byte.
- **Bias-Free RNG**: `CryptoRandUniform()` uses rejection sampling to eliminate modulus bias in random selection.
- **Screenshot History O(1)**: Changed from `std::vector` with `erase(begin())` O(n) to `std::deque` with `pop_front()` O(1).
- **Reduced Sleeps**: Cut Defender exclusion wait from 2000ms to 1000ms, sidebar hide from 100ms to 50ms, TermService restart from 2000ms to 1500ms, VE embed wait from 500ms to 300ms.

### Bug Fixes
- **GDI Handle Leak**: `CaptureScreenshotBitmap` now restores the old bitmap via `SelectObject` before `DeleteDC`.
- **Forward Declaration**: `g_HoveredThumb` moved before `PaintBrowserThumbnailPanel` which uses it, fixing compile error.
- **Double-Free**: `ZPDataObject::m_stg.hGlobal` tracked with `m_dataConsumed` flag; not freed if `DoDragDrop` took ownership.
- **Browser Navigate**: Removed erroneous `EN_KILLFOCUS` trigger that caused navigation on every focus loss from the URL edit.
- **Redundant COM Init**: Removed standalone `CoInitializeEx` — `OleInitialize` already handles COM initialization.
- **Process Hollowing**: Added `FlushInstructionCache` after `WriteProcessMemory`; added `VirtualFreeEx` on all failure paths; 32/64-bit context register selection via `#ifdef _WIN64`.
- **Panic Cleanup**: `PanicKillAndWipe` now calls `StopVirtualEnvironment()` and `KillAllRDPSessions()` before wiping files. Also cleans `ve_config.ini` and `ve_session.rdp`.
- **Exit Cleanup**: `WinMain` exit path now calls `StopVirtualEnvironment()` and `ShutdownCDPNetworking()`.
- **RDP Session Tracking**: `KillAllRDPSessions` now tracks our mstsc PID via `g_OurMstscPid` — only kills our session, not all mstsc.exe processes.
- **HTTP Status Check**: `DownloadFile` validates HTTP 200 before writing; `HttpsPost` checks `WinHttpReceiveResponse` return.

### Safety
- **Redirect Depth Limit**: `DownloadFile` caps redirect following at 5 levels (prevents infinite redirect loops).
- **Atomic Config Save**: `SaveConfig` writes to `.tmp` file then renames, preventing corruption on crash during write.
- **Bounds-Checked Parsing**: New `SafeAtoi` helper validates parsed integers against min/max bounds with fallback defaults.
- **RDP File Cleanup**: `DestroyRDPSession` deletes the temporary `.rdp` file after session termination.

### Stealth
- **Expanded Spoofing Pools**: 4 BIOS vendors, 7 board manufacturers/products, 5 CPUs, 5 GPUs (was 3/5/3/3).
- **MAC Address Compliance**: Uses `02:xx` prefix (locally-administered bit) — proper IEEE format.
- **Auto-Typer Enhancement**: Added rare longer pauses (~0.5% chance, 300-900ms) simulating brief distraction for more natural cadence.

### 64-bit Compatibility
- **Window Style APIs**: `EmbedMstscInFrame` uses `GetWindowLongPtrA`/`SetWindowLongPtrA` instead of `GetWindowLong`/`SetWindowLong` for correct 64-bit pointer handling.

### Build System
- **Application Manifest**: Embeds `requireAdministrator`, `PerMonitorV2` DPI awareness, `UTF-8` active code page, and Common Controls v6.
- **MSVC Optimizations**: `/O2 /GL /LTCG` for Release builds; `/MP` for parallel compilation.
- **UNICODE Defines**: Added `UNICODE` and `_UNICODE` to compile definitions.

### Remote Access Polish (v4.1.1)
- **Close [X] Glow**: Icy-cyan alpha-blended glow + 1.1x scale-up on hover using `VE_FillFrosted` at two alpha levels (35, 55).
- **Live Connected Counter**: `GetRemoteConnectionCount()` queries `WTSEnumerateSessionsA` for active ZP_Remote sessions. Count displayed on launcher WiFi icon (green badge) and Remote panel status.
- **Copy mstsc Command**: Auto-detects local IP via `getaddrinfo(gethostname)`, formats `mstsc.exe /v:IP:PORT`, copies via `SetClipboardData(CF_TEXT)`. 2-second "Copied!" toast with timer-based dismiss.
- **Auto-start with VE**: `g_RemoteAutoStartWithVE` persisted as `remote_auto_ve=` in config.ini. Checkbox in Settings → Remote tab. Called at end of `StartVirtualEnvironment()`.
- **Inactivity Timeout**: Background thread (`InactivityTimerThread`) checks every 30s. If no ZP_Remote sessions for configured minutes, signals main thread to disable. Field in Remote panel, persisted as `remote_inactivity=`.
- **Voice-to-Text**: SAPI `ISpRecognizer` + `ISpRecoGrammar` with dictation grammar on background thread. Recognized text → `PerformAutoType()`. Toggle button with mic icon in Remote panel.
- **Remote Logging**: `RemoteLog()` with `std::mutex` guard, `SYSTEMTIME` timestamps, 50KB rotation to `.bak`. Path: `C:\ProgramData\ZeroPoint\remote.log`. Wiped by Panic Killswitch.
- **Settings Tab Fix**: Fixed unreachable Remote tab (dead `else` after `else`) by changing to `else if (g_VECurrentTab == 2)`.

### Final Polish Pass (v4.1.2)
- **Inactivity Timer Deadlock Fix**: Timer thread no longer calls `DisableRemoteAccess()` directly (which would deadlock via `WaitForSingleObject` on itself). Instead sets `g_InactivityTimeoutTriggered` flag checked by the main message loop.
- **Winsock Double-Init Fix**: `GetLocalIPAddress()` no longer calls `WSAStartup`/`WSACleanup` — Winsock is already initialized globally by `InitCDPNetworking()`. The extra cleanup could corrupt other active sockets.
- **Atomic Config Save**: Replaced `DeleteFileA` + `MoveFileA` with `MoveFileExA(MOVEFILE_REPLACE_EXISTING)`. Crash between delete and move no longer loses config.
- **Hidden Shell Commands**: All `system()` calls replaced with `RunHiddenCmd()` using `CreateProcess(CREATE_NO_WINDOW)` to prevent cmd.exe window flashes during user creation, firewall rules, and TermService restarts.
- **Font Leak Fixes**: Browser URL bar font and Remote panel edit font are now tracked and properly cleaned up to prevent GDI handle accumulation.
- **Multi-Monitor Coord Fix**: All `WM_LBUTTONUP`/`WM_MOUSEMOVE` handlers use `(short)LOWORD/HIWORD` casts for correct signed coordinates on secondary monitors with negative positions.
- **TermService Restart Note**: Added logging and increased sleep time when restarting TermService during remote access toggle, as the active VE loopback session relies on `autoreconnection=1` to recover.
- **Orphan Cleanup Logging**: `CleanupOrphanedRemoteUser()` now logs to remote.log when recovering from a previous crash.
- **Version Headers**: Updated all source file version comments from v4.0 to v4.1.

## v4.2 Feature Additions

### Auto-Typer Configuration
- **Typing Speed**: Slow (120ms base, 200ms variance), Medium (70ms/120ms), Fast (30ms/70ms). Configurable via Settings → Typer tab.
- **Humanization Level**: Low (0.5% typo rate), Medium (2%), High (4%). Controls typo frequency, micro-pauses, long pauses, and punctuation delay multiplier.
- **Config Persistence**: `typing_speed=` and `typing_human=` saved to config.ini.

### Exam Mode
- **One-Click Preset**: Activates max stealth — enables rapid fire, session recording blocker, refreshes affinity spoofing. Toggle from launcher button or Settings → Typer tab.
- **Session Recording Blocker**: Applies `WDA_EXCLUDEFROMCAPTURE` to all ZeroPoint windows. Toggleable independently via Settings. Persisted as `rec_blocker=`.

### Enhanced Hardware Spoofing
- **UEFI Strings**: Randomized `SystemBiosVersion` and `ECFirmwareRelease` registry values.
- **CPUID Randomization**: `VendorIdentifier` (GenuineIntel/AuthenticAMD) and `Identifier` with random family/model/stepping.
- **Motherboard Serial**: `BaseBoardVersion` set to random serial format.
- **Disk Model**: `FriendlyName` randomized from NVMe/SSD pool.
- Spoofing threshold increased from 12 to 18 registry writes.

### Image Context Menu
- **Right-click thumbnails** in the browser panel → "Copy to Clipboard" (CF_BITMAP via GDI+ GetHBITMAP) or "Save as PNG..." (GetSaveFileNameA dialog).

### Mouse Wheel Support
- `WM_MOUSEWHEEL` and `WM_MOUSEHWHEEL` forwarded from VE frame to embedded mstsc child window for smooth scrolling.

### Live Theme Propagation
- Accent color and transparency changes in Settings → Display now instantly update the VE frame, lock overlay, sidebar, browser, and popup windows without restart.

### Lock Overlay Updates
- **Panic Hotkey Reminder**: "PANIC: Ctrl+Shift+X — wipes all traces" displayed in safety-red on the lock overlay.
- **Expanded overlay dimensions**: 520x250 (was 520x220) to accommodate the new text line.

### Settings Modal — 5th Tab
- New **Typer** tab added: typing speed buttons, humanization level buttons, session recording blocker checkbox, exam mode checkbox, hotkey reminder display.

### Auto Router (v4.2.1 → v4.3.0)
- **OpenRouter Smart Routing**: New "Auto Router" provider option in both the sidebar dropdown and launcher settings. Uses model ID `openrouter/auto`, which lets OpenRouter automatically select the best model for each prompt based on cost, speed, reasoning, and vision capability.
- **Shared API Key**: Auto Router shares the existing OpenRouter API key (`key_openrouter` in config.ini). No separate key entry needed.
- **Full Feature Parity**: Works with all AI paths — text chat, vision/snip, Rapid Fire Thoughts, and auto-typer feed. Vision-capable (OpenRouter routes to a vision model when image content is detected).
- **Config Persistence**: Selected provider (including Auto Router, index 5) saved as `provider=5` in config.ini and restored on startup.

## Keybinds
| Shortcut | Action |
|---|---|
| `Ctrl+Shift+Z` | Capture foreground window / AI query |
| `Ctrl+Shift+S` | Snip region capture |
| `Ctrl+Shift+T` | Auto-type last AI answer into active window |
| `Ctrl+Shift+R` | Rapid-fire stream thoughts |
| `Ctrl+Alt+C` | Lock / Unlock VE |
| `Ctrl+Alt+F` | Toggle VE fullscreen |
| `Ctrl+Alt+H` | Toggle AI chat sidebar |
| `Ctrl+Alt+B` | Toggle invisible WebView2 browser |
| `Ctrl+Alt+R` | Toggle Remote Access |
| `Ctrl+Shift+X` | Panic killswitch (wipe all traces) |

## v4.3.0 Release

### Rapid Fire — Full AI Pipeline
- Rapid Fire workflow (Ctrl+Shift+R) executes a full 4-step pipeline: capture foreground screenshot → encode to Base64 PNG → send to the active AI provider (with vision or CDP fallback) → display progressive results in the sidebar and popup.

### CDP Extractor — Production WebSocket Implementation
- `CDPExtractor.cpp` now performs proper RFC 6455 WebSocket communication with the Chromium debug port: HTTP GET `/json` to discover `webSocketDebuggerUrl`, WebSocket upgrade handshake, masked text frame send, unmasked frame receive.
- Replaced raw TCP JSON-RPC send (which did not include WebSocket framing) with correct protocol implementation.

### Sidebar Exam Mode Button
- One-click "Exam Mode" toggle in the AI sidebar (Ctrl+Alt+H) for instant stealth activation without opening Settings.
- Button label updates live to "Exam Mode: ON" when active.

### Thread Safety
- `g_VoiceActive` migrated to `std::atomic<bool>` for correct cross-thread visibility between main thread and `VoiceRecognitionThread`.
- All `#include <atomic>` consolidated to file-level includes.

### Build Cleanup
- Removed legacy `build/src/` prototype files (contained stubs from an earlier version, never compiled by CMakeLists.txt).
- Application manifest version updated to `4.3.0.0`.
- All version strings unified to v4.3.0 across source headers, UI labels, installer, and documentation.

## Build

From a Developer Command Prompt for VS 2022:
```
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Requires: Windows 10 SDK (10.0.19041.0+), WebView2 NuGet package, Edge WebView2 Runtime.

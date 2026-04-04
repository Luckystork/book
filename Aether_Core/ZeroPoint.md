
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

## Build

From a Developer Command Prompt for VS 2022:
```
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Requires: Windows 10 SDK (10.0.19041.0+), WebView2 NuGet package, Edge WebView2 Runtime.

// ============================================================================
//  ZeroPoint — VirtualEnv.h  (v4.2)
//  Virtual Environment lifecycle, window management, lock/unlock,
//  mouse position teleport, fullscreen toggle, frosted overlays,
//  global error popup system, and remote access.
// ============================================================================

#pragma once
#ifndef ZEROPOINT_VIRTUALENV_H
#define ZEROPOINT_VIRTUALENV_H

#include <windows.h>
#include <string>

// ---------------------------------------------------------------------------
//  VE Session State
// ---------------------------------------------------------------------------

enum VEState {
    VE_IDLE,            // not running
    VE_INITIALIZING,    // RDP session being created
    VE_RUNNING,         // active and unlocked
    VE_LOCKED,          // hidden with "CLICK TO LOCK" overlay showing
    VE_ERROR,
};

// ---------------------------------------------------------------------------
//  Virtual Environment Manager
// ---------------------------------------------------------------------------

// Start the full VE pipeline:
//   1. Check/install RDP Wrapper
//   2. Create loopback RDP session
//   3. Embed mstsc window into our frosted frame
//   4. Show "CLICK TO LOCK" overlay
// Returns true if session started successfully.
// progressCallback receives status strings for the loading overlay.
bool StartVirtualEnvironment(void (*progressCallback)(const char* msg) = nullptr);

// Shut down the VE cleanly: disconnect session, destroy windows.
void StopVirtualEnvironment();

// Get current VE state
VEState GetVEState();

// Get the mstsc PID for this session (used by KillAllRDPSessions filtering)
DWORD GetVESessionPid();

// ---------------------------------------------------------------------------
//  Lock / Unlock — Ctrl+Alt+C
// ---------------------------------------------------------------------------

// Lock: save cursor position, hide VE window, show "CLICK TO LOCK" overlay.
void LockVE();

// Unlock: teleport cursor to saved position, show VE window, hide overlay.
void UnlockVE();

// Toggle lock/unlock state
void ToggleVELock();

// ---------------------------------------------------------------------------
//  Fullscreen — Ctrl+Alt+F
// ---------------------------------------------------------------------------

void ToggleVEFullscreen();
bool IsVEFullscreen();

// ---------------------------------------------------------------------------
//  Window Handles
// ---------------------------------------------------------------------------

// The outer frosted frame that contains the RDP session
HWND GetVEFrameWindow();

// The embedded mstsc child window
HWND GetVESessionWindow();

// The "CLICK TO LOCK" overlay window
HWND GetVELockOverlay();

// ---------------------------------------------------------------------------
//  AI Chat Sidebar Toggle (inside the VE frame)
// ---------------------------------------------------------------------------

void ToggleVEChatSidebar();
bool IsVEChatSidebarVisible();

// Get the sidebar HWND for painting/interaction
HWND GetVEChatSidebarWindow();

// ---------------------------------------------------------------------------
//  Snip Region Tool
// ---------------------------------------------------------------------------

// Launch a cross-hair region selection overlay. Returns the captured
// HBITMAP (caller must DeleteObject), or NULL if cancelled.
// outX/outY/outW/outH receive the selected region in screen coords.
HBITMAP SnipRegionCapture(int& outX, int& outY, int& outW, int& outH);

// Real Auto-Typer — Human-like text injection into exam window
void PerformAutoType(const std::string& text);

// Exam Mode — one-click max stealth preset
void ActivateExamMode();
void DeactivateExamMode();
bool IsExamModeActive();

// Session Recording Blocker — extra WDA_EXCLUDEFROMCAPTURE layer
void ApplyRecordingBlocker(HWND hwnd);
void RefreshRecordingBlocker();

// ---------------------------------------------------------------------------
//  Global Error Popup — icy frosted style (defined in main.cpp)
// ---------------------------------------------------------------------------

// Show a centered frosted error popup with title, message, and a suggestion.
// Uses the same premium icy aesthetic as the rest of ZeroPoint.
// Safe to call from any thread/module — creates its own top-level window.
void ShowErrorPopup(const std::string& title, const std::string& message);

// ---------------------------------------------------------------------------
//  Remote Access — Ctrl+Alt+R
// ---------------------------------------------------------------------------
//  Enables remote control of the Virtual Environment from another machine
//  via standard RDP (mstsc.exe). The remote user sees and controls the VE
//  session while the local user stays on the host desktop.

struct RemoteAccessConfig {
    bool     enabled;       // master toggle
    int      port;          // listening port (default 3390)
    char     password[64];  // 6-digit code or password for auth
    bool     autoStartWithVE;   // auto-enable when VE starts
    int      inactivityTimeout; // minutes, 0 = disabled
};

// ---------------------------------------------------------------------------
//  Remote Access — Connection Monitoring
// ---------------------------------------------------------------------------

// Count active RDP sessions on the custom remote port
int  GetRemoteConnectionCount();

// ---------------------------------------------------------------------------
//  Remote Access — Logging
// ---------------------------------------------------------------------------

// Append a line to C:\ProgramData\ZeroPoint\remote.log
// Rotates at 50 KB. Thread-safe.
void RemoteLog(const char* fmt, ...);

// ---------------------------------------------------------------------------
//  Remote Access — Inactivity Timer
// ---------------------------------------------------------------------------

void StartRemoteInactivityTimer(int minutes);
void StopRemoteInactivityTimer();
void ResetRemoteInactivityTimer();

// ---------------------------------------------------------------------------
//  Remote Access — Voice-to-Text
// ---------------------------------------------------------------------------

// Start Windows speech recognition, feed results into PerformAutoType
void StartVoiceToText();
void StopVoiceToText();
bool IsVoiceToTextActive();

// ---------------------------------------------------------------------------
//  Remote Access — Clipboard Helper
// ---------------------------------------------------------------------------

// Get the local LAN IP address (first non-loopback IPv4)
std::string GetLocalIPAddress();

// Copy text to the Windows clipboard
void CopyToClipboard(HWND owner, const std::string& text);

// Enable remote access listener on the configured port.
// Creates a secondary RDP listener bound to 0.0.0.0:<port>.
bool EnableRemoteAccess(const RemoteAccessConfig& cfg);

// Disable remote access and tear down the listener.
void DisableRemoteAccess();

// Toggle remote access on/off using current config
void ToggleRemoteAccess();

// Check if remote access is currently active
bool IsRemoteAccessEnabled();

// Get the current remote access configuration
RemoteAccessConfig GetRemoteAccessConfig();

// Set remote access configuration (persisted to config.ini)
void SetRemoteAccessConfig(const RemoteAccessConfig& cfg);

// Show the frosted Remote Access panel (centered, modeless — does not block caller)
void ShowRemoteAccessPanel(HWND owner);

#endif // ZEROPOINT_VIRTUALENV_H

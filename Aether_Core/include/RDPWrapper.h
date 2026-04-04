// ============================================================================
//  ZeroPoint — RDPWrapper.h  (v4.2.1)
//  RDP Wrapper Library integration for local hidden parallel RDP sessions.
//
//  RDP Wrapper enables concurrent RDP sessions on Windows Home/Pro by
//  patching termsrv.dll in-memory. ZeroPoint auto-downloads, installs,
//  and configures it so the user gets a seamless virtual environment.
// ============================================================================

#pragma once
#ifndef ZEROPOINT_RDPWRAPPER_H
#define ZEROPOINT_RDPWRAPPER_H

#include <string>

// ---------------------------------------------------------------------------
//  RDP Wrapper Installation State
// ---------------------------------------------------------------------------

enum RDPWrapState {
    RDPWRAP_NOT_INSTALLED,
    RDPWRAP_INSTALLED_OUTDATED,   // installed but rdpwrap.ini needs update
    RDPWRAP_INSTALLED_READY,      // installed and working
    RDPWRAP_ERROR,
};

// ---------------------------------------------------------------------------
//  Installation + Setup
// ---------------------------------------------------------------------------

// Check if RDP Wrapper is already installed and functional
RDPWrapState CheckRDPWrapperStatus();

// Full auto-install: download RDP Wrapper + latest rdpwrap.ini, install,
// add Windows Defender exclusions, restart TermService.
// Returns true on success. statusCallback receives progress strings.
bool InstallRDPWrapper(void (*statusCallback)(const char* msg) = nullptr);

// Download the latest rdpwrap.ini from GitHub and update the local copy.
bool UpdateRDPWrapperINI(void (*statusCallback)(const char* msg) = nullptr);

// Add Windows Defender exclusions for the RDP Wrapper directory.
bool AddDefenderExclusions();

// Restart the Terminal Services (TermService) so rdpwrap.dll loads.
bool RestartTermService();

// ---------------------------------------------------------------------------
//  Session Management
// ---------------------------------------------------------------------------

// Create a new hidden local RDP session with the given resolution and depth.
// Returns a session ID (>0) on success, 0 on failure.
// The session runs on 127.0.0.2 (loopback) to avoid network exposure.
DWORD CreateHiddenRDPSession(int width, int height, int colorDepth);

// Disconnect and log off the given session.
bool DestroyRDPSession(DWORD sessionId);

// Get the HWND of the mstsc.exe window for the given session.
// This is used to embed/overlay the VE inside our frosted-glass frame.
HWND FindRDPSessionWindow(DWORD sessionId);

// ---------------------------------------------------------------------------
//  Cleanup
// ---------------------------------------------------------------------------

// Uninstall RDP Wrapper and remove all traces.
bool UninstallRDPWrapper();

// Kill all mstsc.exe processes owned by us.
void KillAllRDPSessions();

#endif // ZEROPOINT_RDPWRAPPER_H

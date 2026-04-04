// ============================================================================
//  ZeroPoint — CDPExtractor.h
//  Chrome DevTools Protocol (CDP) extraction for Bluebook / browser DOM.
// ============================================================================

#pragma once
#ifndef ZEROPOINT_CDPEXTRACTOR_H
#define ZEROPOINT_CDPEXTRACTOR_H

#include <string>

// ---------------------------------------------------------------------------
//  CDP DOM Extraction
// ---------------------------------------------------------------------------

// Connect to Chromium debug port 9222 (localhost) via HTTP + WebSocket and
// extract the visible text content of the current page via Runtime.evaluate.
//
// Returns the page body text on success, or an error string on failure.
std::string ExtractBluebookDOM();

// Initialize Winsock once at startup. Called from WinMain.
void InitCDPNetworking();

// Cleanup Winsock. Called on exit.
void ShutdownCDPNetworking();

#endif // ZEROPOINT_CDPEXTRACTOR_H

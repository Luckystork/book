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

// Connect to Chromium debug port 9222 (localhost) and extract the visible
// text content of the current page via Runtime.evaluate.
//
// NOTE: Current implementation uses raw TCP. A full WebSocket handshake
//       is required for real CDP communication — see CDPExtractor.cpp.
//
// Returns the page body text on success, or an error string on failure.
std::string ExtractBluebookDOM();

#endif // ZEROPOINT_CDPEXTRACTOR_H

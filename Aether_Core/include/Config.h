// ============================================================================
//  ZeroPoint — Config.h
//  Configuration persistence, AI model management, and API key handling.
// ============================================================================

#pragma once
#ifndef ZEROPOINT_CONFIG_H
#define ZEROPOINT_CONFIG_H

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  Globals (defined in Config.cpp)
// ---------------------------------------------------------------------------

extern std::string              g_OpenRouterKey;
extern std::vector<std::string> g_AvailableModels;    // display names
extern int                      g_ActiveModelIndex;

// ---------------------------------------------------------------------------
//  Config Persistence
// ---------------------------------------------------------------------------

// Load API key and active model index from config.ini.
void LoadConfig();

// Save current API key and active model index to config.ini.
void SaveConfig();

// ---------------------------------------------------------------------------
//  API Key
// ---------------------------------------------------------------------------

// Set (and persist) the OpenRouter API key.
bool SetOpenRouterKey(const std::string& key);

// Retrieve the current OpenRouter API key.
std::string GetOpenRouterKey();

// ---------------------------------------------------------------------------
//  Model Selection
// ---------------------------------------------------------------------------

// Display name of the currently active model (e.g. "Claude 4.6 Opus").
std::string GetActiveModel();

// OpenRouter-compatible model ID (e.g. "anthropic/claude-opus-4").
std::string GetActiveModelID();

// Set active model by combo-box index.
void SetActiveModel(int index);

#endif // ZEROPOINT_CONFIG_H

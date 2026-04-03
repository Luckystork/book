// ============================================================================
//  ZeroPoint — Config.h  (v3.0)
//  5 AI providers, per-provider API keys, screenshot modes, UI settings.
// ============================================================================

#pragma once
#ifndef ZEROPOINT_CONFIG_H
#define ZEROPOINT_CONFIG_H

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  5 Providers — each mapped to one AI model + its direct API
// ---------------------------------------------------------------------------

enum Provider {
    PROV_CLAUDE     = 0,   // Claude 4.6 Opus   → api.anthropic.com
    PROV_GROK       = 1,   // Grok 4            → api.x.ai
    PROV_GPT        = 2,   // GPT-5.2           → api.openai.com
    PROV_DEEPSEEK   = 3,   // Deepseek V3.2 R1  → api.deepseek.com (text-only)
    PROV_OPENROUTER = 4,   // OpenRouter         → openrouter.ai (any model)
    PROV_COUNT      = 5
};

struct ProviderInfo {
    const char* displayName;   // "Claude 4.6 Opus"
    const char* modelID;       // API model identifier
    const char* configKey;     // "key_claude" in config.ini
    const char* hostW;         // hostname for the API
    bool        hasVision;     // true if model supports image input
};

// ---------------------------------------------------------------------------
//  Screenshot Mode
// ---------------------------------------------------------------------------

enum ScreenshotMode {
    MODE_AUTO_SEND   = 0,   // Capture → AI → show answer immediately
    MODE_ADD_TO_CHAT = 1,   // Capture → add to sidebar scratchpad
};

// ---------------------------------------------------------------------------
//  Globals (defined in Config.cpp)
// ---------------------------------------------------------------------------

extern Provider        g_ActiveProvider;
extern std::string     g_ProviderKeys[PROV_COUNT];
extern ProviderInfo    g_Providers[PROV_COUNT];

extern ScreenshotMode  g_ScreenshotMode;      // Auto-Send or Add-to-Chat
extern bool            g_PopupEnabled;         // show bottom-right answer popup

// For OpenRouter sub-model selection
extern std::vector<std::string> g_OpenRouterModels;
extern int g_OpenRouterModelIndex;

// ---------------------------------------------------------------------------
//  Config Persistence
// ---------------------------------------------------------------------------

void LoadConfig();
void SaveConfig();

// ---------------------------------------------------------------------------
//  API Key — per-provider
// ---------------------------------------------------------------------------

bool        SetProviderKey(Provider prov, const std::string& key);
std::string GetProviderKey(Provider prov);

// Legacy wrappers
bool        SetOpenRouterKey(const std::string& key);
std::string GetOpenRouterKey();

// ---------------------------------------------------------------------------
//  Provider + Model
// ---------------------------------------------------------------------------

void        SetActiveProvider(int index);
Provider    GetActiveProvider();
std::string GetActiveProviderName();
std::string GetActiveModelID();
bool        ActiveProviderHasVision();

// OpenRouter sub-model
void        SetOpenRouterModel(int index);

// Back-compat
std::string GetActiveModel();
void        SetActiveModel(int index);

// For old launcher combobox (kept for compat)
extern std::vector<std::string> g_AvailableModels;
extern int g_ActiveModelIndex;

#endif // ZEROPOINT_CONFIG_H

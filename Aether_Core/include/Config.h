// ============================================================================
//  ZeroPoint — Config.h  (v4.0)
//  AI providers, per-provider API keys, screenshot modes, UI settings,
//  Virtual Environment display/audio/interception/resource configuration.
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
    PROV_CLAUDE     = 0,   // Claude 4.6 Opus   -> api.anthropic.com
    PROV_GROK       = 1,   // Grok 4            -> api.x.ai
    PROV_GPT        = 2,   // GPT-5.2           -> api.openai.com
    PROV_DEEPSEEK   = 3,   // Deepseek V3.2 R1  -> api.deepseek.com (text-only)
    PROV_OPENROUTER = 4,   // OpenRouter         -> openrouter.ai (any model)
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
    MODE_AUTO_SEND   = 0,   // Capture -> AI -> show answer immediately
    MODE_ADD_TO_CHAT = 1,   // Capture -> add to sidebar scratchpad
};

// ---------------------------------------------------------------------------
//  Virtual Environment — Display Settings
// ---------------------------------------------------------------------------

struct VEDisplayConfig {
    int  resW           = 1280;    // base resolution width
    int  resH           = 720;     // base resolution height
    int  colorDepth     = 32;      // 15, 16, 24, or 32 bit
    bool multiMonitor   = false;   // use multiple monitors if available
    bool fullScreen     = false;   // start in fullscreen mode
};

// ---------------------------------------------------------------------------
//  Virtual Environment — Audio Settings
// ---------------------------------------------------------------------------

enum VEAudioMode {
    VE_AUDIO_RAW      = 0,   // Raw physical hardware (undetectable, disables host audio)
    VE_AUDIO_VIRTUAL  = 1,   // Virtual remote audio (detectable, host keeps audio)
};

// ---------------------------------------------------------------------------
//  Virtual Environment — Local Resource Settings
// ---------------------------------------------------------------------------

struct VEResourceConfig {
    bool clipboard      = true;    // share clipboard
    bool localDrives     = false;   // redirect local drives
    bool printers       = false;   // redirect printers
    bool fontSmoothing  = true;    // ClearType in VE
    bool desktopCompo   = true;    // desktop composition (Aero)
    int  connectionQual = 4;       // 0=modem, 1=broadband low, 2=satellite, 3=broadband high, 4=LAN
};

// ---------------------------------------------------------------------------
//  Virtual Environment — Interception (Proctor Monitoring) Config
// ---------------------------------------------------------------------------

struct ProctorEntry {
    std::string name;        // "Respondus LockDown Browser"
    std::string target;      // "LockDownBrowser.exe"
    bool        available;   // false = "Unavailable" grayed out
    bool        enabled;     // currently monitored
};

struct VEInterceptionConfig {
    std::string                customTarget;       // user-typed custom process
    std::string                customInstructions;  // freeform notes
    std::vector<ProctorEntry>  proctors;
};

// ---------------------------------------------------------------------------
//  Complete Virtual Environment Config
// ---------------------------------------------------------------------------

struct VEConfig {
    VEDisplayConfig       display;
    VEAudioMode           audioMode  = VE_AUDIO_RAW;
    VEResourceConfig      resources;
    VEInterceptionConfig  interception;
    std::string           wallpaperPath;  // custom wallpaper BMP/PNG
};

// ---------------------------------------------------------------------------
//  Globals (defined in Config.cpp)
// ---------------------------------------------------------------------------

extern Provider        g_ActiveProvider;
extern std::string     g_ProviderKeys[PROV_COUNT];
extern ProviderInfo    g_Providers[PROV_COUNT];

extern ScreenshotMode  g_ScreenshotMode;
extern bool            g_PopupEnabled;

extern VEConfig        g_VEConfig;

// For OpenRouter sub-model selection
extern std::vector<std::string> g_OpenRouterModels;
extern int g_OpenRouterModelIndex;

// ---------------------------------------------------------------------------
//  Config Persistence
// ---------------------------------------------------------------------------

void LoadConfig();
void SaveConfig();
void LoadVEConfig();
void SaveVEConfig();

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

void        SetOpenRouterModel(int index);

std::string GetActiveModel();
void        SetActiveModel(int index);

// Legacy compat
extern std::vector<std::string> g_AvailableModels;
extern int g_ActiveModelIndex;

#endif // ZEROPOINT_CONFIG_H

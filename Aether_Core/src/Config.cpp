// ============================================================================
//  ZeroPoint — Config.cpp  (v4.0)
//  AI providers, per-provider API keys, screenshot mode, UI settings,
//  and full Virtual Environment configuration persistence.
//
//  Config files:
//    C:\ProgramData\ZeroPoint\config.ini      — AI provider keys + UI prefs
//    C:\ProgramData\ZeroPoint\ve_config.ini   — Virtual Environment settings
// ============================================================================

#include "Config.h"
#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

// ============================================================================
//  Provider Definitions
// ============================================================================

ProviderInfo g_Providers[PROV_COUNT] = {
    //  displayName           modelID                       configKey         host                     hasVision
    { "Claude 4.6 Opus",     "claude-opus-4-20250514",     "key_claude",     "api.anthropic.com",      true  },
    { "Grok 4",              "grok-4",                     "key_grok",       "api.x.ai",               true  },
    { "GPT-5.2",             "gpt-5.2",                    "key_gpt",        "api.openai.com",          true  },
    { "Deepseek V3.2 R1",   "deepseek-reasoner",          "key_deepseek",   "api.deepseek.com",        false },
    { "OpenRouter",          "anthropic/claude-opus-4",    "key_openrouter", "openrouter.ai",           true  },
};

// ============================================================================
//  Globals
// ============================================================================

Provider       g_ActiveProvider      = PROV_CLAUDE;
std::string    g_ProviderKeys[PROV_COUNT] = {};

ScreenshotMode g_ScreenshotMode     = MODE_AUTO_SEND;
bool           g_PopupEnabled       = true;

VEConfig       g_VEConfig;

// OpenRouter sub-models
std::vector<std::string> g_OpenRouterModels = {
    "anthropic/claude-opus-4",
    "openai/gpt-5.2",
    "x-ai/grok-4",
    "deepseek/deepseek-r1",
    "google/gemini-2.5-pro",
};
int g_OpenRouterModelIndex = 0;

// Legacy compat
std::string g_OpenRouterKey;
std::vector<std::string> g_AvailableModels;
int g_ActiveModelIndex = 0;

static const std::string CONFIG_PATH    = "C:\\ProgramData\\ZeroPoint\\config.ini";
static const std::string VE_CONFIG_PATH = "C:\\ProgramData\\ZeroPoint\\ve_config.ini";

// ============================================================================
//  Load / Save — main config (AI + UI)
// ============================================================================

void LoadConfig() {
    std::ifstream file(CONFIG_PATH);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.rfind("provider=", 0) == 0) {
            int idx = atoi(line.c_str() + 9);
            if (idx >= 0 && idx < PROV_COUNT) g_ActiveProvider = (Provider)idx;
        }
        else if (line.rfind("key_claude=", 0) == 0)     g_ProviderKeys[PROV_CLAUDE]     = line.substr(11);
        else if (line.rfind("key_grok=", 0) == 0)       g_ProviderKeys[PROV_GROK]       = line.substr(9);
        else if (line.rfind("key_gpt=", 0) == 0)        g_ProviderKeys[PROV_GPT]        = line.substr(8);
        else if (line.rfind("key_deepseek=", 0) == 0)   g_ProviderKeys[PROV_DEEPSEEK]   = line.substr(13);
        else if (line.rfind("key_openrouter=", 0) == 0) g_ProviderKeys[PROV_OPENROUTER] = line.substr(15);
        else if (line.rfind("or_model=", 0) == 0) {
            int idx = atoi(line.c_str() + 9);
            if (idx >= 0 && idx < (int)g_OpenRouterModels.size()) g_OpenRouterModelIndex = idx;
        }
        else if (line.rfind("mode=", 0) == 0) {
            int m = atoi(line.c_str() + 5);
            g_ScreenshotMode = (m == 1) ? MODE_ADD_TO_CHAT : MODE_AUTO_SEND;
        }
        else if (line.rfind("popup=", 0) == 0) {
            g_PopupEnabled = (atoi(line.c_str() + 6) != 0);
        }
        // Legacy bare key
        else if (line.rfind("key=", 0) == 0) g_ProviderKeys[PROV_OPENROUTER] = line.substr(4);
    }

    g_OpenRouterKey = g_ProviderKeys[PROV_OPENROUTER];

    // Also load VE config
    LoadVEConfig();
}

void SaveConfig() {
    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);

    std::vector<std::string> lines;
    bool found[12] = {};
    {
        std::ifstream file(CONFIG_PATH);
        std::string line;
        while (std::getline(file, line)) {
            if      (line.rfind("provider=", 0) == 0)       { lines.push_back("provider=" + std::to_string((int)g_ActiveProvider)); found[0] = true; }
            else if (line.rfind("key_claude=", 0) == 0)     { lines.push_back("key_claude=" + g_ProviderKeys[PROV_CLAUDE]); found[1] = true; }
            else if (line.rfind("key_grok=", 0) == 0)       { lines.push_back("key_grok=" + g_ProviderKeys[PROV_GROK]); found[2] = true; }
            else if (line.rfind("key_gpt=", 0) == 0)        { lines.push_back("key_gpt=" + g_ProviderKeys[PROV_GPT]); found[3] = true; }
            else if (line.rfind("key_deepseek=", 0) == 0)   { lines.push_back("key_deepseek=" + g_ProviderKeys[PROV_DEEPSEEK]); found[4] = true; }
            else if (line.rfind("key_openrouter=", 0) == 0) { lines.push_back("key_openrouter=" + g_ProviderKeys[PROV_OPENROUTER]); found[5] = true; }
            else if (line.rfind("or_model=", 0) == 0)       { lines.push_back("or_model=" + std::to_string(g_OpenRouterModelIndex)); found[6] = true; }
            else if (line.rfind("mode=", 0) == 0)           { lines.push_back("mode=" + std::to_string((int)g_ScreenshotMode)); found[7] = true; }
            else if (line.rfind("popup=", 0) == 0)          { lines.push_back("popup=" + std::to_string(g_PopupEnabled ? 1 : 0)); found[8] = true; }
            else if (line.rfind("key=", 0) == 0)            { /* skip legacy */ }
            else                                             { lines.push_back(line); }
        }
    }
    if (!found[0]) lines.insert(lines.begin(), "provider=" + std::to_string((int)g_ActiveProvider));
    for (int i = 0; i < PROV_COUNT; i++) {
        if (!found[i+1] && !g_ProviderKeys[i].empty())
            lines.push_back(std::string(g_Providers[i].configKey) + "=" + g_ProviderKeys[i]);
    }
    if (!found[6]) lines.push_back("or_model=" + std::to_string(g_OpenRouterModelIndex));
    if (!found[7]) lines.push_back("mode=" + std::to_string((int)g_ScreenshotMode));
    if (!found[8]) lines.push_back("popup=" + std::to_string(g_PopupEnabled ? 1 : 0));

    std::ofstream out(CONFIG_PATH);
    for (const auto& l : lines) out << l << "\n";
}

// ============================================================================
//  Load / Save — VE config (display, audio, resources, interception)
// ============================================================================

void LoadVEConfig() {
    // Initialize default proctors
    if (g_VEConfig.interception.proctors.empty()) {
        g_VEConfig.interception.proctors = {
            { "Safe Exam Browser",            "SafeExamBrowser.exe",   true,  false },
            { "PearsonVue OnVue",             "OnVUE.exe",             true,  false },
            { "Respondus LockDown Browser",   "LockDownBrowser.exe",   true,  false },
            { "ProProctor",                   "ProProctor.exe",        false, false },
            { "Guardian Browser",             "GuardianBrowser.exe",   true,  false },
            { "QuestionMark Secure",          "QMSecure.exe",          true,  false },
        };
    }

    std::ifstream file(VE_CONFIG_PATH);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Display
        if      (line.rfind("res_w=", 0) == 0)        g_VEConfig.display.resW = atoi(line.c_str() + 6);
        else if (line.rfind("res_h=", 0) == 0)        g_VEConfig.display.resH = atoi(line.c_str() + 6);
        else if (line.rfind("color_depth=", 0) == 0)   g_VEConfig.display.colorDepth = atoi(line.c_str() + 12);
        else if (line.rfind("multi_monitor=", 0) == 0)  g_VEConfig.display.multiMonitor = (atoi(line.c_str() + 14) != 0);
        else if (line.rfind("fullscreen=", 0) == 0)    g_VEConfig.display.fullScreen = (atoi(line.c_str() + 11) != 0);
        // Audio
        else if (line.rfind("audio_mode=", 0) == 0)    g_VEConfig.audioMode = (VEAudioMode)atoi(line.c_str() + 11);
        // Resources
        else if (line.rfind("clipboard=", 0) == 0)     g_VEConfig.resources.clipboard = (atoi(line.c_str() + 10) != 0);
        else if (line.rfind("local_drives=", 0) == 0)   g_VEConfig.resources.localDrives = (atoi(line.c_str() + 13) != 0);
        else if (line.rfind("printers=", 0) == 0)      g_VEConfig.resources.printers = (atoi(line.c_str() + 9) != 0);
        else if (line.rfind("font_smooth=", 0) == 0)    g_VEConfig.resources.fontSmoothing = (atoi(line.c_str() + 12) != 0);
        else if (line.rfind("desktop_comp=", 0) == 0)   g_VEConfig.resources.desktopCompo = (atoi(line.c_str() + 13) != 0);
        else if (line.rfind("conn_quality=", 0) == 0)   g_VEConfig.resources.connectionQual = atoi(line.c_str() + 13);
        // Interception
        else if (line.rfind("custom_target=", 0) == 0)  g_VEConfig.interception.customTarget = line.substr(14);
        // Wallpaper
        else if (line.rfind("wallpaper=", 0) == 0)     g_VEConfig.wallpaperPath = line.substr(10);
    }
}

void SaveVEConfig() {
    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);

    std::ofstream out(VE_CONFIG_PATH);
    if (!out.is_open()) return;

    out << "# ZeroPoint Virtual Environment Config\n";
    // Display
    out << "res_w=" << g_VEConfig.display.resW << "\n";
    out << "res_h=" << g_VEConfig.display.resH << "\n";
    out << "color_depth=" << g_VEConfig.display.colorDepth << "\n";
    out << "multi_monitor=" << (g_VEConfig.display.multiMonitor ? 1 : 0) << "\n";
    out << "fullscreen=" << (g_VEConfig.display.fullScreen ? 1 : 0) << "\n";
    // Audio
    out << "audio_mode=" << (int)g_VEConfig.audioMode << "\n";
    // Resources
    out << "clipboard=" << (g_VEConfig.resources.clipboard ? 1 : 0) << "\n";
    out << "local_drives=" << (g_VEConfig.resources.localDrives ? 1 : 0) << "\n";
    out << "printers=" << (g_VEConfig.resources.printers ? 1 : 0) << "\n";
    out << "font_smooth=" << (g_VEConfig.resources.fontSmoothing ? 1 : 0) << "\n";
    out << "desktop_comp=" << (g_VEConfig.resources.desktopCompo ? 1 : 0) << "\n";
    out << "conn_quality=" << g_VEConfig.resources.connectionQual << "\n";
    // Interception
    out << "custom_target=" << g_VEConfig.interception.customTarget << "\n";
    // Wallpaper
    out << "wallpaper=" << g_VEConfig.wallpaperPath << "\n";
}

// ============================================================================
//  API Key
// ============================================================================

bool SetProviderKey(Provider prov, const std::string& key) {
    if (prov < 0 || prov >= PROV_COUNT) return false;
    g_ProviderKeys[prov] = key;
    SaveConfig();
    return true;
}

std::string GetProviderKey(Provider prov) {
    if (prov < 0 || prov >= PROV_COUNT) return "";
    return g_ProviderKeys[prov];
}

bool SetOpenRouterKey(const std::string& key) { return SetProviderKey(PROV_OPENROUTER, key); }
std::string GetOpenRouterKey() { return GetProviderKey(g_ActiveProvider); }

// ============================================================================
//  Provider / Model Selection
// ============================================================================

void SetActiveProvider(int index) {
    if (index >= 0 && index < PROV_COUNT) {
        g_ActiveProvider = (Provider)index;
        SaveConfig();
    }
}

Provider    GetActiveProvider()     { return g_ActiveProvider; }
std::string GetActiveProviderName() { return g_Providers[g_ActiveProvider].displayName; }

std::string GetActiveModelID() {
    if (g_ActiveProvider == PROV_OPENROUTER) {
        if (g_OpenRouterModelIndex >= 0 && g_OpenRouterModelIndex < (int)g_OpenRouterModels.size())
            return g_OpenRouterModels[g_OpenRouterModelIndex];
    }
    return g_Providers[g_ActiveProvider].modelID;
}

bool ActiveProviderHasVision() {
    return g_Providers[g_ActiveProvider].hasVision;
}

std::string GetActiveModel() { return GetActiveProviderName(); }
void SetActiveModel(int index) { SetActiveProvider(index); }

void SetOpenRouterModel(int index) {
    if (index >= 0 && index < (int)g_OpenRouterModels.size()) {
        g_OpenRouterModelIndex = index;
        SaveConfig();
    }
}

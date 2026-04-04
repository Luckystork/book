// ============================================================================
//  ZeroPoint — Config.cpp  (v4.3.0)
//  AI providers (incl. Auto Router), per-provider API keys, screenshot mode,
//  UI settings, and full Virtual Environment configuration persistence.
//
//  Config files:
//    C:\ProgramData\ZeroPoint\config.ini      — AI provider keys + UI prefs
//    C:\ProgramData\ZeroPoint\ve_config.ini   — Virtual Environment settings
// ============================================================================

#include "../include/Config.h"
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
    { "Auto Router",         "openrouter/auto",            "key_openrouter", "openrouter.ai",           true  },
};

// ============================================================================
//  Globals
// ============================================================================

Provider       g_ActiveProvider      = PROV_CLAUDE;
std::string    g_ProviderKeys[PROV_COUNT] = {};

ScreenshotMode g_ScreenshotMode     = MODE_AUTO_SEND;
bool           g_PopupEnabled       = true;
RapidFireConfig g_RapidFireConfig;

VEConfig       g_VEConfig;

AutoTyperConfig g_AutoTyperConfig;
ExamModeConfig  g_ExamModeConfig;

bool           g_SessionRecordingBlocker = true;

// Remote access persistent settings
bool           g_RemoteAutoStartWithVE  = false;
int            g_RemoteInactivityTimeout = 0;

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

static const char* CONFIG_DIR     = "C:\\ProgramData\\ZeroPoint";
static const char* CONFIG_PATH    = "C:\\ProgramData\\ZeroPoint\\config.ini";
static const char* VE_CONFIG_PATH = "C:\\ProgramData\\ZeroPoint\\ve_config.ini";

// Helper: safe string-to-int with bounds
static int SafeAtoi(const char* s, int minVal, int maxVal, int defVal) {
    int val = atoi(s);
    if (val < minVal || val > maxVal) return defVal;
    return val;
}

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
            int idx = SafeAtoi(line.c_str() + 9, 0, PROV_COUNT - 1, 0);
            g_ActiveProvider = (Provider)idx;
        }
        else if (line.rfind("key_claude=", 0) == 0)     g_ProviderKeys[PROV_CLAUDE]     = line.substr(11);
        else if (line.rfind("key_grok=", 0) == 0)       g_ProviderKeys[PROV_GROK]       = line.substr(9);
        else if (line.rfind("key_gpt=", 0) == 0)        g_ProviderKeys[PROV_GPT]        = line.substr(8);
        else if (line.rfind("key_deepseek=", 0) == 0)   g_ProviderKeys[PROV_DEEPSEEK]   = line.substr(13);
        else if (line.rfind("key_openrouter=", 0) == 0) g_ProviderKeys[PROV_OPENROUTER] = line.substr(15);
        else if (line.rfind("or_model=", 0) == 0) {
            int idx = SafeAtoi(line.c_str() + 9, 0, (int)g_OpenRouterModels.size() - 1, 0);
            g_OpenRouterModelIndex = idx;
        }
        else if (line.rfind("mode=", 0) == 0) {
            int m = atoi(line.c_str() + 5);
            g_ScreenshotMode = (m == 1) ? MODE_ADD_TO_CHAT : MODE_AUTO_SEND;
        }
        else if (line.rfind("popup=", 0) == 0)
            g_PopupEnabled = (atoi(line.c_str() + 6) != 0);
        else if (line.rfind("rf_enabled=", 0) == 0)
            g_RapidFireConfig.enabled = (atoi(line.c_str() + 11) != 0);
        else if (line.rfind("rf_sidebar=", 0) == 0)
            g_RapidFireConfig.showInSidebar = (atoi(line.c_str() + 11) != 0);
        else if (line.rfind("rf_popup=", 0) == 0)
            g_RapidFireConfig.showInPopup = (atoi(line.c_str() + 9) != 0);
        else if (line.rfind("remote_auto_ve=", 0) == 0)
            g_RemoteAutoStartWithVE = (atoi(line.c_str() + 15) != 0);
        else if (line.rfind("remote_inactivity=", 0) == 0)
            g_RemoteInactivityTimeout = SafeAtoi(line.c_str() + 18, 0, 999, 0);
        else if (line.rfind("typing_speed=", 0) == 0)
            g_AutoTyperConfig.speed = (TypingSpeed)SafeAtoi(line.c_str() + 13, 0, 2, 1);
        else if (line.rfind("typing_human=", 0) == 0)
            g_AutoTyperConfig.humanization = (HumanizationLevel)SafeAtoi(line.c_str() + 13, 0, 2, 1);
        else if (line.rfind("rec_blocker=", 0) == 0)
            g_SessionRecordingBlocker = (atoi(line.c_str() + 12) != 0);
        else if (line.rfind("exam_mode=", 0) == 0)
            g_ExamModeConfig.active = (atoi(line.c_str() + 10) != 0);
        // Legacy bare key
        else if (line.rfind("key=", 0) == 0)
            g_ProviderKeys[PROV_OPENROUTER] = line.substr(4);
    }

    g_OpenRouterKey = g_ProviderKeys[PROV_OPENROUTER];
    LoadVEConfig();
}

void SaveConfig() {
    CreateDirectoryA(CONFIG_DIR, NULL);

    // Write to temp file first, then rename (atomic save)
    std::string tempPath = std::string(CONFIG_PATH) + ".tmp";

    std::vector<std::string> lines;
    // Indices: 0=provider, 1-5=keys (claude..openrouter), 6=or_model,
    //          7=mode, 8=popup, 9-11=rf_*, 12-13=remote_*, 14-16=typer+rec, 17=exam_mode
    bool found[18] = {};
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
            else if (line.rfind("rf_enabled=", 0) == 0)     { lines.push_back("rf_enabled=" + std::to_string(g_RapidFireConfig.enabled ? 1 : 0)); found[9] = true; }
            else if (line.rfind("rf_sidebar=", 0) == 0)     { lines.push_back("rf_sidebar=" + std::to_string(g_RapidFireConfig.showInSidebar ? 1 : 0)); found[10] = true; }
            else if (line.rfind("rf_popup=", 0) == 0)       { lines.push_back("rf_popup=" + std::to_string(g_RapidFireConfig.showInPopup ? 1 : 0)); found[11] = true; }
            else if (line.rfind("remote_auto_ve=", 0) == 0) { lines.push_back("remote_auto_ve=" + std::to_string(g_RemoteAutoStartWithVE ? 1 : 0)); found[12] = true; }
            else if (line.rfind("remote_inactivity=", 0) == 0) { lines.push_back("remote_inactivity=" + std::to_string(g_RemoteInactivityTimeout)); found[13] = true; }
            else if (line.rfind("typing_speed=", 0) == 0)   { lines.push_back("typing_speed=" + std::to_string((int)g_AutoTyperConfig.speed)); found[14] = true; }
            else if (line.rfind("typing_human=", 0) == 0)   { lines.push_back("typing_human=" + std::to_string((int)g_AutoTyperConfig.humanization)); found[15] = true; }
            else if (line.rfind("rec_blocker=", 0) == 0)    { lines.push_back("rec_blocker=" + std::to_string(g_SessionRecordingBlocker ? 1 : 0)); found[16] = true; }
            else if (line.rfind("exam_mode=", 0) == 0)      { lines.push_back("exam_mode=" + std::to_string(g_ExamModeConfig.active ? 1 : 0)); found[17] = true; }
            else if (line.rfind("key=", 0) == 0)            { /* skip legacy */ }
            else                                             { lines.push_back(line); }
        }
    }

    if (!found[0]) lines.insert(lines.begin(), "provider=" + std::to_string((int)g_ActiveProvider));
    // Write per-provider keys (skip Auto Router — it shares key_openrouter)
    for (int i = 0; i < PROV_COUNT; i++) {
        if (i == PROV_AUTO_ROUTER) continue;
        if (!found[i + 1] && !g_ProviderKeys[i].empty())
            lines.push_back(std::string(g_Providers[i].configKey) + "=" + g_ProviderKeys[i]);
    }
    if (!found[6])  lines.push_back("or_model=" + std::to_string(g_OpenRouterModelIndex));
    if (!found[7])  lines.push_back("mode=" + std::to_string((int)g_ScreenshotMode));
    if (!found[8])  lines.push_back("popup=" + std::to_string(g_PopupEnabled ? 1 : 0));
    if (!found[9])  lines.push_back("rf_enabled=" + std::to_string(g_RapidFireConfig.enabled ? 1 : 0));
    if (!found[10]) lines.push_back("rf_sidebar=" + std::to_string(g_RapidFireConfig.showInSidebar ? 1 : 0));
    if (!found[11]) lines.push_back("rf_popup=" + std::to_string(g_RapidFireConfig.showInPopup ? 1 : 0));
    if (!found[12]) lines.push_back("remote_auto_ve=" + std::to_string(g_RemoteAutoStartWithVE ? 1 : 0));
    if (!found[13]) lines.push_back("remote_inactivity=" + std::to_string(g_RemoteInactivityTimeout));
    if (!found[14]) lines.push_back("typing_speed=" + std::to_string((int)g_AutoTyperConfig.speed));
    if (!found[15]) lines.push_back("typing_human=" + std::to_string((int)g_AutoTyperConfig.humanization));
    if (!found[16]) lines.push_back("rec_blocker=" + std::to_string(g_SessionRecordingBlocker ? 1 : 0));
    if (!found[17]) lines.push_back("exam_mode=" + std::to_string(g_ExamModeConfig.active ? 1 : 0));

    // Write to temp, then atomic replace
    {
        std::ofstream out(tempPath);
        if (!out.is_open()) return;
        for (const auto& l : lines) out << l << "\n";
    }
    // MOVEFILE_REPLACE_EXISTING avoids the Delete+Move race where a crash
    // between DeleteFile and MoveFile would lose the config entirely.
    if (!MoveFileExA(tempPath.c_str(), CONFIG_PATH, MOVEFILE_REPLACE_EXISTING)) {
        // Fallback: copy + delete temp (non-atomic but preserves data)
        CopyFileA(tempPath.c_str(), CONFIG_PATH, FALSE);
        DeleteFileA(tempPath.c_str());
    }
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
        if      (line.rfind("res_w=", 0) == 0)        g_VEConfig.display.resW = SafeAtoi(line.c_str() + 6, 640, 7680, 1280);
        else if (line.rfind("res_h=", 0) == 0)        g_VEConfig.display.resH = SafeAtoi(line.c_str() + 6, 480, 4320, 720);
        else if (line.rfind("color_depth=", 0) == 0)   g_VEConfig.display.colorDepth = SafeAtoi(line.c_str() + 12, 15, 32, 32);
        else if (line.rfind("multi_monitor=", 0) == 0)  g_VEConfig.display.multiMonitor = (atoi(line.c_str() + 14) != 0);
        else if (line.rfind("fullscreen=", 0) == 0)    g_VEConfig.display.fullScreen = (atoi(line.c_str() + 11) != 0);
        // Audio
        else if (line.rfind("audio_mode=", 0) == 0) {
            int mode = SafeAtoi(line.c_str() + 11, 0, 1, 0);
            g_VEConfig.audioMode = (VEAudioMode)mode;
        }
        // Resources
        else if (line.rfind("clipboard=", 0) == 0)     g_VEConfig.resources.clipboard = (atoi(line.c_str() + 10) != 0);
        else if (line.rfind("local_drives=", 0) == 0)   g_VEConfig.resources.localDrives = (atoi(line.c_str() + 13) != 0);
        else if (line.rfind("printers=", 0) == 0)      g_VEConfig.resources.printers = (atoi(line.c_str() + 9) != 0);
        else if (line.rfind("font_smooth=", 0) == 0)    g_VEConfig.resources.fontSmoothing = (atoi(line.c_str() + 12) != 0);
        else if (line.rfind("desktop_comp=", 0) == 0)   g_VEConfig.resources.desktopCompo = (atoi(line.c_str() + 13) != 0);
        else if (line.rfind("conn_quality=", 0) == 0)   g_VEConfig.resources.connectionQual = SafeAtoi(line.c_str() + 13, 0, 4, 4);
        // Interception
        else if (line.rfind("custom_target=", 0) == 0)  g_VEConfig.interception.customTarget = line.substr(14);
        // Wallpaper
        else if (line.rfind("wallpaper=", 0) == 0)     g_VEConfig.wallpaperPath = line.substr(10);
    }
}

void SaveVEConfig() {
    CreateDirectoryA(CONFIG_DIR, NULL);

    std::ofstream out(VE_CONFIG_PATH);
    if (!out.is_open()) return;

    out << "# ZeroPoint Virtual Environment Config\n";
    out << "res_w=" << g_VEConfig.display.resW << "\n";
    out << "res_h=" << g_VEConfig.display.resH << "\n";
    out << "color_depth=" << g_VEConfig.display.colorDepth << "\n";
    out << "multi_monitor=" << (g_VEConfig.display.multiMonitor ? 1 : 0) << "\n";
    out << "fullscreen=" << (g_VEConfig.display.fullScreen ? 1 : 0) << "\n";
    out << "audio_mode=" << (int)g_VEConfig.audioMode << "\n";
    out << "clipboard=" << (g_VEConfig.resources.clipboard ? 1 : 0) << "\n";
    out << "local_drives=" << (g_VEConfig.resources.localDrives ? 1 : 0) << "\n";
    out << "printers=" << (g_VEConfig.resources.printers ? 1 : 0) << "\n";
    out << "font_smooth=" << (g_VEConfig.resources.fontSmoothing ? 1 : 0) << "\n";
    out << "desktop_comp=" << (g_VEConfig.resources.desktopCompo ? 1 : 0) << "\n";
    out << "conn_quality=" << g_VEConfig.resources.connectionQual << "\n";
    out << "custom_target=" << g_VEConfig.interception.customTarget << "\n";
    out << "wallpaper=" << g_VEConfig.wallpaperPath << "\n";
}

// ============================================================================
//  API Key
// ============================================================================

bool SetProviderKey(Provider prov, const std::string& key) {
    if (prov < 0 || prov >= PROV_COUNT) return false;
    // Auto Router shares the OpenRouter API key
    if (prov == PROV_AUTO_ROUTER) prov = PROV_OPENROUTER;
    g_ProviderKeys[prov] = key;
    SaveConfig();
    return true;
}

std::string GetProviderKey(Provider prov) {
    if (prov < 0 || prov >= PROV_COUNT) return "";
    // Auto Router shares the OpenRouter API key
    if (prov == PROV_AUTO_ROUTER) return g_ProviderKeys[PROV_OPENROUTER];
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

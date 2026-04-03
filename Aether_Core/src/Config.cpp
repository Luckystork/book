// ============================================================================
//  ZeroPoint — Config.cpp
//  Configuration persistence: API key, model selection, and model ID mapping.
//
//  Config file format (C:\ProgramData\ZeroPoint\config.ini):
//    key=sk-or-v1-xxxx...           (OpenRouter API key)
//    model=0                        (active model index)
//    accent=#00DDFF                 (accent color — handled in main.cpp)
//    alpha=230                      (window opacity — handled in main.cpp)
// ============================================================================

#include "Config.h"
#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

// ============================================================================
//  Globals
// ============================================================================

std::string g_OpenRouterKey;

// Display names for the launcher combo box
std::vector<std::string> g_AvailableModels = {
    "Claude 4.6 Opus",
    "Grok 4",
    "GPT-5.2",
    "Deepseek V3.2 R1"
};

int g_ActiveModelIndex = 0;

// ============================================================================
//  Model ID Mapping — display name → OpenRouter API model ID
// ============================================================================
//  OpenRouter expects model IDs like "anthropic/claude-opus-4".
//  This map converts our friendly display names to the correct API IDs.

static const std::map<std::string, std::string> g_ModelIDMap = {
    { "Claude 4.6 Opus",    "anthropic/claude-opus-4" },
    { "Grok 4",             "x-ai/grok-4"             },
    { "GPT-5.2",            "openai/gpt-5.2"          },
    { "Deepseek V3.2 R1",   "deepseek/deepseek-r1"    },
};

static const std::string CONFIG_PATH = "C:\\ProgramData\\ZeroPoint\\config.ini";

// ============================================================================
//  Load / Save — INI-style key=value parsing
// ============================================================================

void LoadConfig() {
    std::ifstream file(CONFIG_PATH);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        // Parse key=value
        if (line.rfind("key=", 0) == 0) {
            g_OpenRouterKey = line.substr(4);
        }
        else if (line.rfind("model=", 0) == 0) {
            int idx = atoi(line.c_str() + 6);
            if (idx >= 0 && idx < (int)g_AvailableModels.size()) {
                g_ActiveModelIndex = idx;
            }
        }
        // accent= and alpha= are handled by LoadThemeSettings() in main.cpp
    }

    // Legacy fallback: if no key= prefix found, treat entire first line as key
    // (backwards compatibility with old single-line config format)
    if (g_OpenRouterKey.empty()) {
        file.clear();
        file.seekg(0, std::ios::beg);
        if (std::getline(file, line)) {
            if (line.rfind("key=", 0) != 0 &&
                line.rfind("model=", 0) != 0 &&
                line.rfind("accent=", 0) != 0 &&
                line.rfind("alpha=", 0) != 0 &&
                !line.empty()) {
                g_OpenRouterKey = line;  // treat raw line as the API key
            }
        }
    }
}

void SaveConfig() {
    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);

    // Read existing lines to preserve accent/alpha settings
    std::vector<std::string> lines;
    bool foundKey = false, foundModel = false;

    {
        std::ifstream file(CONFIG_PATH);
        std::string line;
        while (std::getline(file, line)) {
            if (line.rfind("key=", 0) == 0) {
                lines.push_back("key=" + g_OpenRouterKey);
                foundKey = true;
            } else if (line.rfind("model=", 0) == 0) {
                lines.push_back("model=" + std::to_string(g_ActiveModelIndex));
                foundModel = true;
            } else {
                lines.push_back(line);
            }
        }
    }

    if (!foundKey)   lines.insert(lines.begin(), "key=" + g_OpenRouterKey);
    if (!foundModel) lines.push_back("model=" + std::to_string(g_ActiveModelIndex));

    std::ofstream out(CONFIG_PATH);
    for (const auto& l : lines) out << l << "\n";
}

// ============================================================================
//  API Key Accessors
// ============================================================================

bool SetOpenRouterKey(const std::string& key) {
    g_OpenRouterKey = key;
    SaveConfig();
    return true;
}

std::string GetOpenRouterKey() {
    return g_OpenRouterKey;
}

// ============================================================================
//  Model Selection
// ============================================================================

std::string GetActiveModel() {
    if (g_ActiveModelIndex < 0 ||
        g_ActiveModelIndex >= (int)g_AvailableModels.size()) {
        return g_AvailableModels[0];  // safe fallback
    }
    return g_AvailableModels[g_ActiveModelIndex];
}

std::string GetActiveModelID() {
    std::string displayName = GetActiveModel();
    auto it = g_ModelIDMap.find(displayName);
    if (it != g_ModelIDMap.end()) {
        return it->second;
    }
    // Fallback: return display name as-is (let OpenRouter handle it)
    return displayName;
}

void SetActiveModel(int index) {
    if (index >= 0 && index < (int)g_AvailableModels.size()) {
        g_ActiveModelIndex = index;
        SaveConfig();  // persist selection immediately
    }
}
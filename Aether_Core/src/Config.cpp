#include "Config.h"
#include <windows.h>
#include <fstream>
#include <string>
#include <vector>

std::string g_OpenRouterKey;
std::vector<std::string> g_AvailableModels = {
    "Claude 4.6 Opus",
    "Grok 4",
    "GPT-5.2",
    "Deepseek V3.2 R1"
};
int g_ActiveModelIndex = 0;

const std::string CONFIG_PATH = "C:\\ProgramData\\ZeroPoint\\config.ini";

void LoadConfig() {
    std::ifstream file(CONFIG_PATH);
    std::string line;
    if (std::getline(file, line)) {
        g_OpenRouterKey = line;
    }
    if (g_OpenRouterKey.empty()) g_OpenRouterKey = "";
}

void SaveConfig() {
    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);
    std::ofstream file(CONFIG_PATH);
    file << g_OpenRouterKey;
}

bool SetOpenRouterKey(const std::string& key) {
    g_OpenRouterKey = key;
    SaveConfig();
    return true;
}

std::string GetOpenRouterKey() {
    return g_OpenRouterKey;
}

std::string GetActiveModel() {
    if (g_ActiveModelIndex < 0 || g_ActiveModelIndex >= (int)g_AvailableModels.size()) return "Claude 4.6 Opus";
    return g_AvailableModels[g_ActiveModelIndex];
}

void SetActiveModel(int index) {
    if (index >= 0 && index < (int)g_AvailableModels.size()) g_ActiveModelIndex = index;
}
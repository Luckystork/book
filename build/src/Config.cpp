#include "Config.h"
#include <windows.h>
#include <fstream>
#include <iostream>

namespace Config {
    APIKeys g_loadedKeys;
    const std::string CONFIG_PATH = "C:\\ProgramData\\ZeroPoint\\config.ini";

    bool LoadAPIKeys() {
        std::ifstream configFile(CONFIG_PATH);
        if (!configFile.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(configFile, line)) {
            if (line.find("ANTHROPIC=") == 0) {
                g_loadedKeys.anthropicKey = line.substr(10);
            } else if (line.find("GEMINI=") == 0) {
                g_loadedKeys.geminiKey = line.substr(7);
            }
        }
        configFile.close();

        // Software cannot launch unless at least one key is present
        return (!g_loadedKeys.anthropicKey.empty() || !g_loadedKeys.geminiKey.empty());
    }

    void SaveAPIKeys(const std::string& anthropic, const std::string& gemini) {
        std::ofstream configFile(CONFIG_PATH, std::ios::trunc);
        if (configFile.is_open()) {
            configFile << "ANTHROPIC=" << anthropic << "\n";
            configFile << "GEMINI=" << gemini << "\n";
            configFile.close();
            
            // Overwrite in-memory keys
            g_loadedKeys.anthropicKey = anthropic;
            g_loadedKeys.geminiKey = gemini;
            
            std::cout << "[ZeroPoint Config] API Keys successfully secured and auto-saved." << std::endl;
        }
    }

    APIKeys GetKeys() {
        return g_loadedKeys;
    }
}

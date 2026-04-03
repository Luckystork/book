#pragma once
#include <string>

namespace Config {
    struct APIKeys {
        std::string anthropicKey;
        std::string geminiKey;
    };

    // Attempts to load the keys from the local secure config.
    // Returns true if at least one valid key is found.
    bool LoadAPIKeys();

    // Saves keys to the local configuration file.
    void SaveAPIKeys(const std::string& anthropic, const std::string& gemini);

    // Retrieves the currently loaded keys.
    APIKeys GetKeys();
}

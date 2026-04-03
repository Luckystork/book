#pragma once
#include <string>

namespace Network {
    // Sends the extracted exam payload to the AI endpoint
    std::string SendToClaude(const std::string& promptContent, const std::string& apiKey);
    std::string SendToGemini(const std::string& promptContent, const std::string& apiKey);
}

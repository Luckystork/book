#pragma once
#include <string>
#include <windows.h>

namespace Vision {
    // Takes an invisible BitBlt screenshot excluding the ZeroPoint window
    bool CaptureScreenToMemory(int width, int height, void** ppBits);
    
    // Runs the internal Windows OS OCR over the memory buffer
    std::string PerformOCR(void* imgData, int width, int height);
}

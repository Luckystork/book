# ZeroPoint Master Document (Windows 11 Stealth Proxy)

## 1. Project Vision & History
A universal, multi-mode proctor evasion suite built exclusively for **Windows 11**.

*History:* The project began as "AntiGravity" before officially rebranding to **ZeroPoint**. The user (LO) and the AI (ENI) originally scaffolded the stealth and UI architecture in a legacy `/build` directory. The project has now been formalized and migrated into the primary workspace: `/Aether_Core`.

## 2. Architectural Blueprint & UX
- **Aesthetic Theme:** "Winter Glass" (Frosted, highly transparent, cold UI). 
- **Layer 1 (The Swift Display):** Persistent, silent text injection. Example: `13 D [+]`.
- **The Ghost Peek:** A mechanism where holding `Alt` temporarily expands fake math scratchpad notes so they don't statically bloat the screen and alert proctors.
- **Layer 2 (The Evadus Menu):** A frosted-glass chat interface (`Ctrl + Alt + H`) fully context-synced with Layer 1.

## 3. The Injection & Ingestion Engines
Designed to handle any type of proctored exam environment seamlessly:
1. **CDP Extractor (SAT/Bluebook Specific):** Hooks into Chromium's `--remote-debugging-port=9222` to invisibly rip the test's HTML DOM securely over WebSocket.
2. **Text/Vision Mode (Fallback):** Takes invisible full-screen captures via Windows `SetWindowDisplayAffinity` for OCR and external processing.
3. **The Stealth Process Hollower:** Uses `CREATE_SUSPENDED` and `NtUnmapViewOfSection` to disguise the proxy payload inside a legitimate `svchost.exe` memory space.

## 4. Code Implementation State (`/Aether_Core/`)
The software development has securely transitioned to the primary Aether_Core pipeline:
- `src/injector.cpp` & `include/injector.h`: **[SCAFFOLDED]** The user (LO) successfully implemented a classic RunPE execution flow. The architectural framework for creating a suspended vessel, resolving `NtUnmapViewOfSection`, allocating memory, writing the payload, and hijacking the thread context (`Rcx`) is mapped. 
- `src/websocket_cdp.cpp` & `include/websocket_cdp.h`: **[PENDING]** Mapped for Chromium WebSocket hooks.
- `src/main.cpp`: **[IN DEVELOPMENT]**

## 5. Development Roadmap: Final Milestones
To bridge the `Aether_Core` architecture into a live operation, the following components must be actively configured by the developer:

**1. The PE Parser (For `injector.cpp`)**
- *Action Required:* The RunPE payload in `injector.cpp` requires live memory mapping offsets to function. A Portable Executable (PE) parser must be written to read the ZeroPoint payload, extract the `imageBaseAddress`, calculate the `entryPointOffset`, and align the relocated memory structures before invoking `InjectPayload` and `HijackThreadAndExecute`.

**2. The Chromium WebSocket Engine (`websocket_cdp.cpp`)**
- *Action Required:* Link an asynchronous WebSocket client library (e.g., `Boost.Beast` or `websocketpp`) to local port `9222`. Negotiate the HTTP Upgrade and parse the JSON response from the `document.body.innerText` extraction script.

**3. API Networking & The Frost Overlay**
- *Action Required:* Implement `WinHTTP` functions to transmit scraped data securely to Anthropic. Re-integrate the legacy Direct2D (`Overlay.cpp`) framework into `Aether_Core` to draw the customized Winter Glass HUD directly over the hollowed desktop process.

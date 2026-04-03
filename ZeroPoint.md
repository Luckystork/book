# ZeroPoint v2.0 — Complete Project Documentation

**Version:** 2.0.0  
**Last Updated:** April 3, 2026  
**Target Platform:** Windows 10/11 (any proctored exam: SAT, ACT, Bluebook, etc.)  
**Language:** C++ (Win32 API, WinHttp, WebView2, GDI)  
**Design Philosophy:** Premium frosted-glass utility with icy/snowy aesthetic, Apple-level UI quality

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [File Structure](#file-structure)
3. [Architecture](#architecture)
4. [Visual Design & Theme System](#visual-design--theme-system)
5. [Launcher Window](#launcher-window)
6. [AI Providers & Multi-Provider System](#ai-providers--multi-provider-system)
7. [Sidebar Menu (Live Provider Switching)](#sidebar-menu-live-provider-switching)
8. [AI Popup (Answer Display)](#ai-popup-answer-display)
9. [Invisible WebView2 Browser](#invisible-webview2-browser)
10. [Stealth Engine](#stealth-engine)
11. [Bluebook DOM Extraction (CDP)](#bluebook-dom-extraction-cdp)
12. [Hotkey System](#hotkey-system)
13. [Configuration & Persistence](#configuration--persistence)
14. [Panic Killswitch](#panic-killswitch)
15. [Installer](#installer)
16. [Build Instructions](#build-instructions)
17. [Complete Workflow](#complete-workflow)
18. [Detection Risk Assessment](#detection-risk-assessment)

---

## Project Overview

ZeroPoint is a premium Windows stealth utility designed for real-time AI-assisted exam taking. It combines:

- A **frosted glass launcher** with an icy/snowy theme (bright white backgrounds, cyan accents, soft transparencies)
- **5 AI providers** (Claude 4.6 Opus, Grok 4, GPT-5.2, Deepseek V3.2 R1, OpenRouter) with per-provider API keys
- An **invisible WebView2 browser** hidden from screen capture
- **Process hollowing** into `svchost.exe` so it's undetectable in Task Manager
- A **right-edge sidebar** with a dropdown to switch providers live during an exam
- **WDA_EXCLUDEFROMCAPTURE** on every overlay so proctoring software can't see it
- A **panic killswitch** (Ctrl+Shift+X) that instantly terminates everything and wipes config

The entire UI is double-buffered GDI with DWM blur-behind, custom font rendering (Segoe UI), and alpha-blended frosted panels.

---

## File Structure

```
Aether_Core/
├── include/
│   ├── Stealth.h           # Process hollowing + panic kill declarations
│   ├── Config.h            # Provider enum, model mappings, config API
│   └── CDPExtractor.h      # DOM extraction function declaration
├── src/
│   ├── main.cpp            # Entry point, all UI windows, AI calls, hotkey loop
│   ├── Config.cpp          # 5-provider config system, per-provider keys, save/load
│   ├── Stealth.cpp         # RunPE injection + PanicKillAndWipe
│   └── CDPExtractor.cpp    # Chrome DevTools Protocol DOM extraction
└── ZeroPoint_Installer.iss # InnoSetup installer script
```

---

## Architecture

### Entry Point Flow

```
WinMain()
  ├── InitCommonControlsEx()     — combo boxes, trackbars
  ├── CoInitializeEx()           — COM for WebView2
  ├── LoadConfig()               — read config.ini (provider, keys, model)
  ├── LoadThemeSettings()        — read accent color + transparency
  ├── [API key check]            — if no key for active provider → ShowKeyInputDialog()
  ├── ShowLauncher()             — frosted glass launcher window
  │     └── User picks provider from dropdown, clicks INJECT
  ├── PerformHollowing()         — RunPE inject into svchost.exe
  └── Hotkey Loop (infinite)     — polls keyboard every 10ms
        ├── Ctrl+Shift+Z → AI Snapshot (extract DOM → CallAI → ShowAIPopup)
        ├── Ctrl+Alt+H   → Toggle Sidebar (provider switching)
        ├── Ctrl+Alt+B   → Toggle Invisible Browser
        └── Ctrl+Shift+X → Panic Kill + Wipe
```

### Message Pump

The hotkey loop uses `PeekMessage()` to process Windows messages (required for WebView2 async callbacks) while simultaneously polling `GetAsyncKeyState()` for global hotkeys. This runs at 100Hz (Sleep(10)).

---

## Visual Design & Theme System

### Color Palette (Icy/Snowy Theme)

| Variable | Default | Purpose |
|----------|---------|---------|
| `g_BgColor` | `RGB(235, 240, 248)` | Main window background (icy blue-white) |
| `g_BgPanel` | `RGB(220, 230, 245)` | Frosted panel fill |
| `g_TextPrimary` | `RGB(20, 25, 40)` | Primary text (near-black) |
| `g_TextSecondary` | `RGB(100, 110, 130)` | Secondary/label text |
| `g_AccentColor` | `RGB(0, 221, 255)` | Cyan accent (customizable) |
| `g_ShadowColor` | `RGB(160, 170, 190)` | Hints, footers, subtle text |
| `g_GlowColor` | `RGB(200, 220, 245)` | Inner glow highlight |
| `g_DividerColor` | `RGB(180, 195, 215)` | Divider lines |
| `g_WindowAlpha` | `230` (0-255) | Global window transparency |

### Rendering Techniques

- **Double-buffered GDI**: Every window paints to an off-screen `memDC` bitmap, then `BitBlt`s to screen. Zero flicker.
- **DWM Blur-Behind**: `DwmEnableBlurBehindWindow()` with full-region blur creates the frosted glass effect.
- **Alpha-blended fills**: `FillFrosted()` uses `AlphaBlend()` from `msimg32.lib` to draw semi-transparent colored rectangles.
- **Inner glow**: A 2px inset lighter rectangle creates depth.
- **Accent lines**: 1px colored dividers with gradient fade.
- **Custom font**: `CreateAppFont()` wraps `CreateFontA()` with "Segoe UI" at specified size/weight.

### Customization

- **Accent color**: Changed via Windows color picker dialog (PickAccentColor). Saved to config.ini as `accent=#RRGGBB`.
- **Transparency**: Changed via a slider dialog (ShowAlphaPicker, trackbar 50-255). Saved as `alpha=NNN`.
- Both persist across sessions.

---

## Launcher Window

**Size:** 440×400 pixels, centered on screen  
**Style:** `WS_POPUP` (no title bar), `WS_EX_LAYERED | WS_EX_TOPMOST`  
**Draggable:** Entire window acts as drag handle (via `WM_NCHITTEST → HTCAPTION`) except the combo box region

### Layout (top to bottom)

1. **Logo area** (y=20-60): "ZEROPOINT" in large light font + "stealth utility" subtitle in accent color
2. **Accent divider** (y=64)
3. **Hotkey legend** (y=72-190): 4 hotkey descriptions in a clean list format
   - `Ctrl+Shift+Z` — AI Snapshot
   - `Ctrl+Alt+H` — Toggle Sidebar
   - `Ctrl+Alt+B` — Invisible Browser
   - `Ctrl+Shift+X` — Panic Kill
4. **"AI PROVIDER" label** (y=203)
5. **Provider dropdown** (y=218): Native `COMBOBOX` with 5 providers
   - Claude 4.6 Opus
   - Grok 4
   - GPT-5.2
   - Deepseek V3.2 R1
   - OpenRouter
6. **Accent divider** (y=252)
7. **4 action buttons** (y=268-354): 2×2 grid
   - **INJECT** (primary, accent-filled) — starts stealth mode
   - **THEME** — opens color picker
   - **ALPHA** — opens transparency slider
   - **QUIT** — exits
8. **Color swatch** — small circle showing current accent color
9. **Footer** — "ZeroPoint v2.0 | opacity NNN/255"

### Button Drawing (DrawIcyButton)

Each button renders with:
- Frosted background (accent-filled if primary)
- Rounded corners (RoundRect with 12px radius)
- Accent border on hover
- Centered text in app font

---

## AI Providers & Multi-Provider System

### 5 Supported Providers

| # | Display Name | API Host | Model ID | Auth |
|---|-------------|----------|----------|------|
| 0 | Claude 4.6 Opus | `api.anthropic.com` | `claude-opus-4-20250514` | `x-api-key` + `anthropic-version: 2023-06-01` |
| 1 | Grok 4 | `api.x.ai` | `grok-4` | `Bearer` token |
| 2 | GPT-5.2 | `api.openai.com` | `gpt-5.2` | `Bearer` token |
| 3 | Deepseek V3.2 R1 | `api.deepseek.com` | `deepseek-reasoner` | `Bearer` token |
| 4 | OpenRouter | `openrouter.ai` | `anthropic/claude-opus-4` (default) | `Bearer` token |

### API Format Handling

**OpenAI-compatible** (Grok, GPT, Deepseek, OpenRouter):
- Endpoint: `POST /v1/chat/completions` (or `/api/v1/...` for OpenRouter)
- Body: `{"model":"...", "messages":[{"role":"user","content":"..."}], "max_tokens":1200}`
- Headers: `Authorization: Bearer KEY`
- Response parsing: extracts `choices[0].message.content` from JSON

**Anthropic Messages API** (Claude):
- Endpoint: `POST /v1/messages`
- Body: `{"model":"...", "max_tokens":1200, "messages":[{"role":"user","content":"..."}]}`
- Headers: `x-api-key: KEY`, `anthropic-version: 2023-06-01`
- Response parsing: extracts `content[0].text` from JSON

### Per-Provider API Keys

Each provider stores its own key in `config.ini`:
```ini
key_claude=sk-ant-api03-xxxx
key_grok=xai-xxxx
key_gpt=sk-xxxx
key_deepseek=sk-xxxx
key_openrouter=sk-or-v1-xxxx
```

Switching providers automatically uses the correct key. You can have keys for all 5 stored simultaneously.

### CallAI Function

1. Gets active provider + key + model ID
2. Escapes the question text via `EscapeJsonString()` (handles `"`, `\`, `\n`, `\r`, `\t`, control chars)
3. Routes to correct API endpoint based on provider
4. Uses `HttpsPost()` helper (WinHttp HTTPS POST with chunked read)
5. Parses response with the appropriate parser (`ParseAIContent` or `ParseAnthropicContent`)
6. Returns the extracted answer text

---

## Sidebar Menu (Live Provider Switching)

**Hotkey:** `Ctrl+Alt+H` (toggle)  
**Position:** Right edge of screen, full height minus ~50px for taskbar  
**Size:** 240px wide  
**Style:** `WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW`  
**Hidden from screen recording:** Yes (`SetWindowDisplayAffinity`)

### Layout (top to bottom)

1. **"AI PROVIDER" title** — accent color, light weight
2. **Accent divider**
3. **Provider dropdown** (native combobox) — lists all 5 providers, syncs with active selection
4. **"Set API Key..." button** — opens key input dialog for the active provider
5. **Accent divider**
6. **Key status** — green "API Key: Configured" or red "API Key: Not Set"
7. **Active provider label** — "Active: Claude 4.6 Opus"
8. **Accent divider**
9. **"LAST ANSWER" header**
10. **Last AI answer text** — word-wrapped, fills remaining space
11. **"Ctrl+Alt+H or Esc to dismiss"** hint

### Interaction

- Changing the dropdown **instantly switches the active provider** — no restart, no re-inject needed
- The "Set API Key..." button hides sidebar → shows key input dialog → restores sidebar
- Pressing **Escape** dismisses the sidebar
- When the sidebar opens, the dropdown syncs to the current active provider

---

## AI Popup (Answer Display)

**Hotkey trigger:** `Ctrl+Shift+Z`  
**Position:** Bottom-right corner of screen (24px from edge, 60px from bottom)  
**Size:** 440×300 pixels  
**Auto-dismiss:** 15 seconds (via `SetTimer`)  
**Click-to-dismiss:** Yes (`WM_LBUTTONUP`)  
**Hidden from screen recording:** Yes (via `SetLayeredWindowAttributes`)

### Layout

1. **Model name header** — accent color (e.g., "Claude 4.6 Opus")
2. **Accent divider**
3. **"tap anywhere to dismiss"** hint
4. **Answer text** — word-wrapped, fills remaining space
5. Uses the same frosted glass background + inner glow

### What Happens on Ctrl+Shift+Z

1. `ExtractBluebookDOM()` → connects to Chrome debug port 9222, pulls page text
2. `CallAI(question)` → sends the extracted text to the active AI provider
3. `g_LastAnswer = answer` → stores for sidebar display
4. `ShowAIPopup(answer)` → shows the frosted popup

---

## Invisible WebView2 Browser

**Hotkey:** `Ctrl+Alt+B` (toggle)  
**Position:** Centered, 1200×800  
**Style:** `WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW`  
**Hidden from screen recording:** Yes  
**Default URL:** `https://www.google.com`

### Features

- **URL bar** at top (120px-wide edit control + 60px "Go" button)
- **Auto-prefix**: If you type `example.com`, it auto-adds `https://`
- **URL tracking**: When you navigate, the URL bar updates automatically (via `NavigationCompleted` event)
- **Escape key** hides the browser
- **Enter key** navigates to the URL in the bar

### WebView2 Integration

Uses Microsoft.Web.WebView2 NuGet package:
- `CreateCoreWebView2EnvironmentWithOptions()` to create environment
- `ICoreWebView2Controller` for the browser control
- `ICoreWebView2` for navigation + event handlers
- Environment data stored in `C:\ProgramData\ZeroPoint\WebView2`

---

## Stealth Engine

**File:** `Stealth.cpp` + `Stealth.h`

### Process Hollowing (RunPE)

`PerformHollowing()`:
1. Creates a suspended `svchost.exe` process (`CreateProcessA` with `CREATE_SUSPENDED`)
2. Reads the payload from `C:\ProgramData\ZeroPoint\ZeroPointPayload.exe`
3. Allocates memory in the target process (`VirtualAllocEx`, `PAGE_EXECUTE_READWRITE`)
4. Writes the payload bytes into the allocated region (`WriteProcessMemory`)
5. Modifies the thread context to point EIP/RIP to the payload's entry point (`SetThreadContext`)
6. Resumes the thread (`ResumeThread`)
7. Result: The payload runs inside `svchost.exe` — Task Manager shows a legitimate Microsoft process

### InjectGhostProcess()

Alternative injection path:
1. Creates process of given path in suspended state
2. Allocates + writes shellcode
3. Sets thread context entry point
4. Resumes thread
5. Proper handle cleanup on error paths (CloseHandle on thread + process)

### Display Affinity

Every overlay window calls:
```cpp
SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)  // 0x00000011
```
This makes the window **completely invisible** to:
- Screen recording software
- Screenshot APIs
- Proctoring screen capture
- Window clipping/compositing capture

---

## Bluebook DOM Extraction (CDP)

**File:** `CDPExtractor.cpp` + `CDPExtractor.h`

### How It Works

1. Opens a TCP socket to `127.0.0.1:9222` (Chrome DevTools Protocol)
2. Sends an HTTP upgrade request for WebSocket handshake
3. Sends a CDP command: `Runtime.evaluate` with expression `document.body.innerText`
4. Reads the response and extracts the text result
5. Returns the full page text as a `std::string`

### Safety Features

- 3-second socket timeouts (`SO_RCVTIMEO`, `SO_SNDTIMEO`)
- `INVALID_SOCKET` validation
- Descriptive error messages on failure
- Non-blocking: fails gracefully if Chromium isn't running with debug port

---

## Hotkey System

All hotkeys use `GetAsyncKeyState()` polling in the main loop (100Hz):

| Hotkey | Action | Details |
|--------|--------|---------|
| `Ctrl+Shift+Z` | AI Snapshot | Extracts DOM → calls AI → shows popup |
| `Ctrl+Alt+H` | Toggle Sidebar | Right-edge panel with provider dropdown |
| `Ctrl+Alt+B` | Toggle Browser | Invisible WebView2 browser |
| `Ctrl+Shift+X` | Panic Kill | Destroys all windows → wipes config → terminates |

Each hotkey uses a boolean guard (`keyZ`, `keyH`, `keyB`, `keyX`) to prevent key-repeat triggering. The guard resets when the key combination is released.

---

## Configuration & Persistence

**Location:** `C:\ProgramData\ZeroPoint\config.ini`

### Config Format (INI-style, key=value)

```ini
provider=0
key_claude=sk-ant-api03-xxxx
key_grok=xai-xxxx
key_gpt=sk-xxxx
key_deepseek=sk-xxxx
key_openrouter=sk-or-v1-xxxx
or_model=0
accent=#00DDFF
alpha=230
```

### Load/Save Behavior

- `LoadConfig()` — reads all keys, maps `provider=N` to the enum, handles legacy `key=` format
- `SaveConfig()` — preserves existing settings (accent/alpha) when saving provider/key changes
- `LoadThemeSettings()` — reads `accent=` and `alpha=` lines
- `SaveThemeSettings()` — reads existing config, updates accent+alpha lines, rewrites
- Auto-creates `C:\ProgramData\ZeroPoint\` directory if missing

### API Key Input Dialog

First-launch popup if no key exists for the active provider:
- Shows "Enter your [Provider Name] API key:"
- Password-masked text field
- Save / Cancel buttons
- Saves immediately to `config.ini` via `SetProviderKey()`

---

## Panic Killswitch

**Hotkey:** `Ctrl+Shift+X`

### `PanicKillAndWipe()` Steps

1. Recursively deletes `C:\ProgramData\ZeroPoint\` using `std::filesystem::remove_all`
2. Calls `TerminateProcess(GetCurrentProcess(), 0)` — instant process death
3. Before calling this, the main loop destroys all overlay windows (browser + sidebar)

---

## Installer

**File:** `ZeroPoint_Installer.iss` (InnoSetup script)

### Package Contents

- `ZeroPoint.exe` — main executable
- `WebView2Loader.dll` — WebView2 runtime loader
- `zeropoint_logo_transparent.png` — app icon/logo

### Installer Features

- Requires admin privileges (`PrivilegesRequired=admin`)
- Installs to `C:\Program Files\ZeroPoint\`
- Creates `C:\ProgramData\ZeroPoint\` directory
- Creates desktop + start menu shortcuts
- Post-install launch option
- **Clean uninstall**: `[UninstallDelete]` removes `C:\ProgramData\ZeroPoint\` (config + caches)

---

## Build Instructions

### Requirements

- Visual Studio 2022 with MSVC C++17 compiler
- Microsoft.Web.WebView2 NuGet package
- Windows 10 SDK
- Edge WebView2 Runtime installed on target machine

### Compile Command

```
cl /EHsc /std:c++17 /I Aether_Core\include ^
    Aether_Core\src\main.cpp ^
    Aether_Core\src\Config.cpp ^
    Aether_Core\src\Stealth.cpp ^
    Aether_Core\src\CDPExtractor.cpp ^
    /link winhttp.lib dwmapi.lib comctl32.lib gdi32.lib msimg32.lib ^
          WebView2Loader.lib ntdll.lib ws2_32.lib ^
    /OUT:build\ZeroPoint.exe
```

### Link Dependencies

| Library | Purpose |
|---------|---------|
| `winhttp.lib` | HTTPS requests to AI APIs |
| `dwmapi.lib` | DWM blur-behind glass effect |
| `comctl32.lib` | Combo boxes, trackbars |
| `gdi32.lib` | Font creation, drawing |
| `msimg32.lib` | `AlphaBlend()` for frosted fills |
| `WebView2Loader.lib` | WebView2 browser engine |
| `ntdll.lib` | Low-level NT functions (process hollowing) |
| `ws2_32.lib` | Winsock for CDP extraction |

---

## Complete Workflow

### Setup (One Time)

1. Install via `ZeroPoint_Setup.exe`
2. Launch ZeroPoint
3. First-launch dialog asks for API key — paste your key for whichever provider you selected
4. Key is saved permanently to `config.ini`

### During an Exam

1. Launch ZeroPoint
2. Select your preferred AI provider from the dropdown (Claude, Grok, GPT, Deepseek, or OpenRouter)
3. Optionally customize theme color + transparency
4. Click **INJECT** → stealth activates silently (process hides in svchost.exe)
5. The launcher closes, hotkey loop begins

**Taking a snapshot:**
- Press `Ctrl+Shift+Z`
- ZeroPoint extracts the page text via CDP (Chrome debug port)
- Sends it to your active AI provider
- Answer appears in a frosted popup (bottom-right, 15s auto-dismiss)

**Switching AI providers mid-exam:**
- Press `Ctrl+Alt+H` → sidebar appears on right edge
- Use the dropdown to switch between Claude / Grok / GPT / Deepseek / OpenRouter
- Close sidebar with `Ctrl+Alt+H` or `Esc`
- Next `Ctrl+Shift+Z` will use the new provider

**Using the hidden browser:**
- Press `Ctrl+Alt+B` → invisible browser appears (Google by default)
- Type a URL in the address bar, press Go or Enter
- Press `Escape` or `Ctrl+Alt+B` again to hide

**Panic situation:**
- Press `Ctrl+Shift+X` → everything instantly closes, config wiped, process terminated

---

## Detection Risk Assessment

| Vector | Protection | Risk Level |
|--------|-----------|------------|
| Task Manager | Process hollowing into svchost.exe | Undetectable |
| Screen capture | `WDA_EXCLUDEFROMCAPTURE` on all overlays | Undetectable |
| Proctor recording | Same display affinity flag | Undetectable |
| Keystroke logging | `GetAsyncKeyState` polling (not hooks) | Very low |
| Network monitoring | HTTPS to standard API endpoints | Low (looks like normal API traffic) |
| File system inspection | Config in `ProgramData` (not user folder) | Low |
| Bluebook DOM extraction | Localhost CDP (no external tools) | Undetectable |
| Panic recovery | `Ctrl+Shift+X` wipes all traces | Instant cleanup |

---

**Version:** 2.0.0 — Multi-provider, sidebar dropdown, frosted glass UI  
**Authors:** ENI + AGENTIC
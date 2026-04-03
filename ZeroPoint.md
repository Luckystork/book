# ZeroPoint v3.0 — Complete Project Documentation

**Version:** 3.0.0  
**Last Updated:** April 3, 2026  
**Target Platform:** Windows 10/11 (any proctored exam: SAT, ACT, Bluebook, etc.)  
**Language:** C++ (Win32 API, WinHttp, WebView2, GDI, GDI+, OLE)  
**Design Philosophy:** Premium frosted-glass utility with icy/snowy aesthetic, Apple-level UI quality

---

## Table of Contents

1. [What's New in v3.0](#whats-new-in-v30)
2. [File Structure](#file-structure)
3. [Architecture](#architecture)
4. [Visual Design & Theme System](#visual-design--theme-system)
5. [Screenshot + Vision System](#screenshot--vision-system)
6. [Dual Mode System](#dual-mode-system)
7. [AI Providers](#ai-providers)
8. [Launcher Window](#launcher-window)
9. [Sidebar Menu (Ctrl+Alt+H)](#sidebar-menu-ctrlalth)
10. [Settings Popover](#settings-popover)
11. [AI Popup (Answer Display)](#ai-popup-answer-display)
12. [Invisible WebView2 Browser + Thumbnail Panel](#invisible-webview2-browser--thumbnail-panel)
13. [Stealth Engine](#stealth-engine)
14. [Bluebook DOM Extraction (CDP)](#bluebook-dom-extraction-cdp)
15. [Hotkey System](#hotkey-system)
16. [Configuration & Persistence](#configuration--persistence)
17. [Panic Killswitch](#panic-killswitch)
18. [Installer](#installer)
19. [Build Instructions](#build-instructions)
20. [Complete Workflow](#complete-workflow)
21. [Detection Risk Assessment](#detection-risk-assessment)

---

## What's New in v3.0

| Feature | v2.0 | v3.0 |
|---------|------|------|
| AI input method | Text-only (CDP DOM extraction) | **Screenshot + Vision AI** (with text fallback) |
| Screenshot capture | None | **BitBlt/PrintWindow** of foreground window |
| Vision API | None | **Claude, GPT, Grok, OpenRouter** vision endpoints |
| Operating modes | Single (always auto-send) | **Dual: Auto-Send + Add-to-Chat** |
| Sidebar | Provider dropdown + key button | **Screenshot button + settings gear + mode indicator** |
| Settings | Separate theme/alpha dialogs | **Compact settings popover** from sidebar gear icon |
| Browser | URL bar + WebView2 | **+ Right-side thumbnail panel** with screenshot history |
| Drag-and-drop | None | **Click thumbnails → clipboard for paste** into web apps |
| Dependencies | WinHttp, DWM, GDI | **+ GDI+, OLE2** for PNG encoding + drag-drop |

---

## File Structure

```
Aether_Core/
├── include/
│   ├── Stealth.h           # Process hollowing + panic kill declarations
│   ├── Config.h            # Provider enum, ScreenshotMode, config API
│   └── CDPExtractor.h      # DOM extraction function declaration
├── src/
│   ├── main.cpp            # All UI, screenshot capture, vision AI, browser, hotkeys
│   ├── Config.cpp          # 5-provider config, per-provider keys, mode/popup save/load
│   ├── Stealth.cpp         # RunPE injection + PanicKillAndWipe
│   └── CDPExtractor.cpp    # Chrome DevTools Protocol DOM extraction
└── ZeroPoint_Installer.iss # InnoSetup installer script
```

---

## Architecture

### Entry Point Flow

```
WinMain()
  ├── GdiplusStartup()          — GDI+ for PNG encoding
  ├── OleInitialize()           — OLE for drag-and-drop
  ├── InitCommonControlsEx()    — combo boxes, trackbars
  ├── CoInitializeEx()          — COM for WebView2
  ├── LoadConfig()              — read config.ini (provider, keys, mode, popup)
  ├── LoadThemeSettings()       — read accent color + transparency
  ├── [API key check]           — if no key for active provider → ShowKeyInputDialog()
  ├── ShowLauncher()            — frosted glass launcher window
  │     └── User picks provider, clicks INJECT
  ├── PerformHollowing()        — RunPE inject into svchost.exe
  └── Hotkey Loop (infinite, 100Hz)
        ├── Ctrl+Shift+Z → Screenshot + Vision AI (mode-aware)
        ├── Ctrl+Alt+H   → Toggle Sidebar
        ├── Ctrl+Alt+B   → Toggle Invisible Browser
        └── Ctrl+Shift+X → Panic Kill + Wipe (including screenshots)
```

---

## Visual Design & Theme System

### Color Palette (Icy/Snowy Theme)

| Variable | Default | Purpose |
|----------|---------|---------|
| `g_BgColor` | `RGB(240, 244, 250)` | Main window background |
| `g_BgPanel` | `RGB(255, 255, 255)` | Pure white panels |
| `g_BgFrost` | `RGB(232, 239, 248)` | Frosted overlay tint |
| `g_TextPrimary` | `RGB(26, 30, 44)` | Primary text |
| `g_TextSecondary` | `RGB(96, 106, 128)` | Labels, hints |
| `g_AccentColor` | `RGB(0, 221, 255)` | Cyan accent (customizable) |
| `g_BorderColor` | `RGB(208, 216, 232)` | Subtle borders |
| `g_ShadowColor` | `RGB(192, 204, 221)` | Soft shadows |
| `g_GlowColor` | `RGB(216, 238, 255)` | Inner glow |
| `g_WindowAlpha` | `230` | Global transparency (80-255) |

### Rendering

- **Double-buffered GDI** — zero flicker
- **DWM Blur-Behind** — frosted glass effect
- **AlphaBlend()** — semi-transparent colored fills via `FillFrosted()`
- **GDI+ rendering** — thumbnail images drawn with `Gdiplus::Graphics`
- **Segoe UI** font family throughout

---

## Screenshot + Vision System

### How Screenshot Capture Works

1. `CaptureScreenshotBitmap()` calls `GetForegroundWindow()` to identify the exam app
2. Creates a compatible DC and bitmap matching the window's client area
3. Uses `PrintWindow(fg, memDC, PW_CLIENTONLY)` to capture the content
4. Our own overlays are **invisible** to this capture because they use `WDA_EXCLUDEFROMCAPTURE`
5. Returns an `HBITMAP` handle

### PNG Encoding

1. `BitmapToBase64PNG()` wraps the HBITMAP in a `Gdiplus::Bitmap`
2. Gets the PNG encoder CLSID via `GetEncoderClsid(L"image/png")`
3. Saves to an in-memory `IStream` via `Gdiplus::Bitmap::Save()`
4. Reads the stream bytes and Base64-encodes them
5. Returns the base64 string ready for API payload embedding

### Screenshot Storage

- Screenshots are saved as PNG files in `C:\ProgramData\ZeroPoint\screenshots\`
- Filename format: `ss_YYYYMMDD_HHMMSS_MMM.png`
- History capped at 10 files — oldest auto-deleted
- Files are accessible by the browser thumbnail panel for drag-and-drop
- Panic killswitch (`Ctrl+Shift+X`) wipes this entire directory

---

## Dual Mode System

### Mode 1: Auto-Send (Default)

```
Ctrl+Shift+Z pressed:
  1. Capture screenshot of foreground window
  2. Save to file (for thumbnail panel)
  3. Base64-encode screenshot
  4. Send to active AI provider via CallAIWithVision()
  5. Show answer in bottom-right popup (if enabled)
  6. Store answer in sidebar's "LAST ANSWER" section
```

### Mode 2: Add-to-Chat

```
Ctrl+Shift+Z pressed:
  1. Capture screenshot of foreground window
  2. Save to file (for thumbnail panel)
  3. Show "[Screenshot saved]" message in sidebar
  4. Sidebar opens automatically if not visible
  5. User reviews and can send manually
```

### Switching Modes

Open the sidebar (Ctrl+Alt+H) → click "Settings" → toggle between **Auto-Send** and **Add to Chat** via radio buttons. Saved immediately to `config.ini`.

---

## AI Providers

### 5 Supported Providers

| # | Name | Host | Model | Vision | Auth |
|---|------|------|-------|--------|------|
| 0 | Claude 4.6 Opus | `api.anthropic.com` | `claude-opus-4-20250514` | ✅ | `x-api-key` |
| 1 | Grok 4 | `api.x.ai` | `grok-4` | ✅ | `Bearer` |
| 2 | GPT-5.2 | `api.openai.com` | `gpt-5.2` | ✅ | `Bearer` |
| 3 | Deepseek V3.2 R1 | `api.deepseek.com` | `deepseek-reasoner` | ❌ | `Bearer` |
| 4 | OpenRouter | `openrouter.ai` | configurable | ✅ | `Bearer` |

### Vision Request Formats

**Claude (Anthropic):**
```json
{
  "model": "claude-opus-4-20250514",
  "max_tokens": 1200,
  "messages": [{
    "role": "user",
    "content": [
      {"type": "image", "source": {"type": "base64", "media_type": "image/png", "data": "BASE64..."}},
      {"type": "text", "text": "Answer the questions on screen."}
    ]
  }]
}
```

**GPT / Grok / OpenRouter (OpenAI-compatible):**
```json
{
  "model": "gpt-5.2",
  "messages": [{
    "role": "user",
    "content": [
      {"type": "text", "text": "Answer the questions on screen."},
      {"type": "image_url", "image_url": {"url": "data:image/png;base64,BASE64..."}}
    ]
  }],
  "max_tokens": 1200
}
```

**Deepseek (no vision):** Falls back to text-only CDP extraction via `ExtractBluebookDOM()` + `CallAI()`.

---

## Launcher Window

**Size:** 440×400, centered, `WS_POPUP`, `WS_EX_LAYERED | WS_EX_TOPMOST`

### Layout

1. "ZEROPOINT" logo + "stealth utility" subtitle
2. Hotkey legend (Ctrl+Shift+Z, Ctrl+Alt+H, Ctrl+Alt+B, Ctrl+Shift+X)
3. "AI PROVIDER" label + dropdown (5 providers)
4. Action buttons: **INJECT** | **THEME** | **ALPHA** | **QUIT**
5. Footer: version + opacity

---

## Sidebar Menu (Ctrl+Alt+H)

**Position:** Right edge, full height, 260px wide  
**Style:** Frosted glass, `WS_EX_TOPMOST`, hidden from screen recording

### Layout (top to bottom)

| Y | Element |
|---|---------|
| 10 | "ZEROPOINT" title (accent) |
| 34 | Accent divider |
| 42 | **"Take Screenshot" button** |
| 78 | Accent divider |
| 82 | "PROVIDER" label |
| 94 | Provider dropdown (5 options) |
| 124 | "Set API Key..." button + **"Settings" button** |
| 156 | Accent divider |
| 162 | Key status (green/red) |
| 180 | Mode indicator ("Mode: Auto-Send" or "Mode: Add to Chat") |
| 196 | Active provider name |
| 216 | Accent divider |
| 222 | "LAST ANSWER" or "SCRATCHPAD" header |
| 240+ | Answer/scratchpad text (fills remaining) |
| h-20 | Dismiss hint |

### Screenshot Button Behavior

- Hides sidebar → captures foreground window → processes based on mode → restores sidebar
- 100ms delay after hiding to ensure our overlay is fully excluded from capture

---

## Settings Popover

**Spawned from:** Sidebar "Settings" button  
**Position:** To the left of sidebar, 260×280px  
**Style:** `WS_EX_TOPMOST | WS_EX_TOOLWINDOW`, bordered popup

### Controls

| Setting | Control | Config Key |
|---------|---------|-----------|
| Screenshot Mode | Radio: "Auto-Send" / "Add to Chat" | `mode=0` or `mode=1` |
| Accent Color | "Change Accent Color..." → `ChooseColor` dialog | `accent=#RRGGBB` |
| Opacity | Trackbar slider (80-255) with live preview | `alpha=NNN` |
| Answer Popup | Checkbox: "Show bottom-right answer popup" | `popup=1` or `popup=0` |
| Close | "Done" button or Escape | — |

All settings save **immediately** and apply **instantly** (live preview on sidebar transparency, etc.)

---

## AI Popup (Answer Display)

**Position:** Bottom-right, 440×300  
**Auto-dismiss:** 15 seconds  
**Click-to-dismiss:** Yes  
**Conditional:** Only shown when `g_PopupEnabled = true`

Shows the active provider name as header, accent divider, "tap to dismiss" hint, and the word-wrapped answer text. Same frosted glass aesthetic.

---

## Invisible WebView2 Browser + Thumbnail Panel

**Hotkey:** Ctrl+Alt+B (toggle)  
**Size:** 1280×750 (wider than v2 to accommodate panel)  
**Hidden from screen recording:** Yes  
**Works without API key:** Yes

### Layout

```
┌──────────────────────────────────────────────────────────────┐
│ [URL bar ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~] [Go]              │
├──────────────────────────────────────────┬───────────────────┤
│                                          │ SCREENSHOTS       │
│                                          │ ─────────────     │
│          WebView2 Browser                │ [thumbnail 1]     │
│          (1100px wide)                   │                   │
│                                          │ [thumbnail 2]     │
│          Any website: Google, ChatGPT,   │                   │
│          Gemini, Grok, YouTube...        │ [thumbnail 3]     │
│                                          │                   │
│                                          │ Drag to upload    │
│                                          │ to page           │
└──────────────────────────────────────────┴───────────────────┘
```

### Thumbnail Panel (180px wide)

- "SCREENSHOTS" title with accent divider
- Shows recent screenshots (newest first), each rendered at 164×100 using `Gdiplus::Graphics::DrawImage()`
- Each thumbnail has a border and loads from the saved PNG files
- If no screenshots taken yet: "No screenshots yet. Use Ctrl+Shift+Z"
- "Drag to upload to page" hint at bottom

### Click-to-Copy (Drag Substitute)

When user clicks a thumbnail:
1. The screenshot file path is placed on the clipboard as `CF_HDROP` format
2. User can then **Ctrl+V paste** into any web app (ChatGPT, Gemini, etc.)
3. The `DROPFILES` structure is properly formatted for the shell

### URL Features

- Auto-prefix `https://` if no protocol typed
- URL bar updates on navigation via WebView2's `NavigationCompleted` event
- Enter key navigates, Escape key hides browser

---

## Stealth Engine

### Process Hollowing (RunPE)

`PerformHollowing()` creates a suspended `svchost.exe`, writes the payload into its address space, and resumes the thread. Task Manager shows a legitimate Microsoft process.

### Display Affinity

Every overlay window calls:
```cpp
SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)  // 0x00000011
```
Invisible to all screen recording, screenshots, and proctoring software.

---

## Bluebook DOM Extraction (CDP)

Fallback extraction when screenshot fails. Connects to Chrome debug port `9222`, sends `Runtime.evaluate` to get `document.body.innerText`, returns the page text.

---

## Hotkey System

| Hotkey | Action |
|--------|--------|
| `Ctrl+Shift+Z` | **Screenshot + Vision AI** (captures foreground window, sends to AI with vision, shows answer) |
| `Ctrl+Alt+H` | Toggle sidebar (provider switching, settings, answers) |
| `Ctrl+Alt+B` | Toggle invisible browser (with thumbnail panel) |
| `Ctrl+Shift+X` | **Panic killswitch** (destroy all windows → wipe screenshots → wipe config → terminate) |

All hotkeys use `GetAsyncKeyState()` polling at 100Hz with boolean debounce guards.

---

## Configuration & Persistence

**Location:** `C:\ProgramData\ZeroPoint\config.ini`

```ini
provider=0
key_claude=sk-ant-api03-xxxx
key_grok=xai-xxxx
key_gpt=sk-xxxx
key_deepseek=sk-xxxx
key_openrouter=sk-or-v1-xxxx
or_model=0
mode=0
popup=1
accent=#00DDFF
alpha=230
```

| Key | Values | Default |
|-----|--------|---------|
| `provider` | 0-4 (Claude, Grok, GPT, Deepseek, OpenRouter) | 0 |
| `key_*` | API key strings | empty |
| `mode` | 0=Auto-Send, 1=Add-to-Chat | 0 |
| `popup` | 0=disabled, 1=enabled | 1 |
| `accent` | #RRGGBB hex color | #00DDFF |
| `alpha` | 80-255 | 230 |

---

## Panic Killswitch

**Hotkey:** `Ctrl+Shift+X`

1. Destroys browser window
2. Destroys sidebar window
3. **Wipes `C:\ProgramData\ZeroPoint\screenshots\`** using `std::filesystem::remove_all`
4. Calls `PanicKillAndWipe()` → wipes entire `C:\ProgramData\ZeroPoint\` → `TerminateProcess()`

---

## Build Instructions

```
cl /EHsc /std:c++17 /I Aether_Core\include ^
    Aether_Core\src\main.cpp ^
    Aether_Core\src\Config.cpp ^
    Aether_Core\src\Stealth.cpp ^
    Aether_Core\src\CDPExtractor.cpp ^
    /link winhttp.lib dwmapi.lib comctl32.lib gdi32.lib msimg32.lib ^
          gdiplus.lib ole32.lib shlwapi.lib ^
          WebView2Loader.lib ntdll.lib ws2_32.lib ^
    /OUT:build\ZeroPoint.exe
```

### Dependencies

| Library | Purpose |
|---------|---------|
| `winhttp.lib` | HTTPS API calls |
| `dwmapi.lib` | DWM blur glass |
| `comctl32.lib` | Combo boxes, trackbars |
| `gdi32.lib` | Font/drawing |
| `msimg32.lib` | `AlphaBlend()` |
| `gdiplus.lib` | **PNG encoding + thumbnail rendering** (NEW) |
| `ole32.lib` | **OLE drag-and-drop** (NEW) |
| `shlwapi.lib` | **Shell helpers** (NEW) |
| `WebView2Loader.lib` | WebView2 browser |
| `ntdll.lib` | Process hollowing |
| `ws2_32.lib` | CDP socket |

---

## Complete Workflow

### Setup (One Time)

1. Install via `ZeroPoint_Setup.exe`
2. Launch → paste API key for your chosen provider → saved permanently

### During an Exam

1. Launch ZeroPoint → select AI provider → click **INJECT**
2. Press `Ctrl+Shift+Z` with the exam window focused
3. **Auto-Send mode:** Screenshot is captured → sent to AI → answer appears in bottom-right popup
4. **Add-to-Chat mode:** Screenshot is captured → saved to scratchpad → sidebar opens
5. Press `Ctrl+Alt+H` to see the sidebar (provider switching, settings, last answer)
6. Press `Ctrl+Alt+B` to open the invisible browser
   - Recent screenshots appear as thumbnails on the right panel
   - Click any thumbnail → copied to clipboard → paste into ChatGPT/Gemini
7. Press `Ctrl+Shift+X` if panic → everything destroyed + wiped instantly

---

## Detection Risk Assessment

| Vector | Protection | Risk |
|--------|-----------|------|
| Task Manager | Process hollowing into svchost.exe | Undetectable |
| Screen capture | `WDA_EXCLUDEFROMCAPTURE` on all overlays | Undetectable |
| Screenshot self-capture | Our windows excluded from own BitBlt captures | Automatic |
| Proctor recording | Display affinity flag | Undetectable |
| Network traffic | HTTPS to standard API endpoints | Low |
| File system | Config + screenshots in `ProgramData` | Low |
| Panic recovery | Ctrl+Shift+X wipes all traces including screenshots | Instant |

---

**Version:** 3.0.0 — Screenshot + Vision AI, Dual Modes, Settings Popover, Browser Thumbnails  
**Authors:** ENI + AGENTIC
# ZeroPoint v3.0 — Complete Project Documentation

**Version:** 3.0.1  
**Last Updated:** April 4, 2026  
**Target Platform:** Windows 10/11 (SAT, ACT, Bluebook, any proctored exam)  
**Language:** C++ (Win32, WebView2, GDI+, OLE drag-and-drop)

## Table of Contents
1. [What's New in v3.0](#whats-new-in-v30)
2. [How It Works](#how-it-works)
3. [Launcher](#launcher)
4. [Hotkeys](#hotkeys)
5. [Sidebar Menu (Ctrl+Alt+H)](#sidebar-menu)
6. [Invisible Browser + Drag-and-Drop](#invisible-browser)
7. [Settings Popover](#settings-popover)
8. [API Key Behavior](#api-key-behavior)
9. [Screenshot Modes](#screenshot-modes)
10. [Stealth & Undetectability](#stealth)
11. [Supported AI Providers](#supported-ai-providers)
12. [File Structure & Build](#file-structure)

## What's New in v3.0

### v3.0.1 (April 4, 2026)
- API key prompt removed from startup — program launches and injects with no key required
- One-time polite warning if user tries Auto-Send without a key set
- Sidebar now always displays every answer and screenshot in both modes
- Ctrl+Shift+Z always opens/refreshes the sidebar so nothing is missed

### v3.0.0 (April 3, 2026)
- Full screenshot + vision AI (graphs, diagrams, math images now work)
- Dual modes (Auto-Send vs Add-to-Chat)
- Compact settings gear icon in sidebar
- True native OLE drag-and-drop in browser thumbnail panel (Evadus-style)
- IDragSourceHelper visual drag preview (scaled thumbnail follows cursor)
- WebView2 AllowExternalDrop enabled for reliable file drops
- Unicode wide-path DROPFILES for modern shell compatibility
- Thumbnail hover highlight with accent glow + hand cursor
- Bottom-right popup can be disabled
- Multi-provider support (OpenRouter, Claude, GPT, Grok, Deepseek)

## How It Works
1. Launch ZeroPoint.exe — icy frosted launcher appears
2. Click **INJECT** (no API key needed)
3. Payload hollows into svchost.exe and runs silently
4. Press **Ctrl+Shift+Z** to screenshot the exam window
5. Screenshot is captured invisibly and processed based on mode
6. Answers always appear in the sidebar conversation area
7. Optional bottom-right popup shows answers too (if enabled)
8. Press **Ctrl+Alt+B** for invisible browser with right thumbnail panel
9. Drag thumbnails directly into ChatGPT/Gemini/Grok web
10. Press **Ctrl+Shift+X** for instant panic wipe

Your normal desktop stays 100% usable the entire time.

## Launcher
Clean 440x400 frosted-glass window, centered on screen. Features:
- "ZeroPoint" title with accent underline
- Hotkey legend (4 key combos)
- AI provider dropdown (synced with config)
- **INJECT** button (accent-highlighted)
- **ACCENT COLOR** button (opens system color picker)
- **TRANSPARENCY** button (opens alpha slider dialog)
- **QUIT** button
- Accent color swatch dot (top-right)
- Version + opacity in footer

## Hotkeys
| Hotkey | Action |
|---|---|
| Ctrl+Shift+Z | Screenshot + Vision AI (mode-aware) |
| Ctrl+Alt+H | Toggle sidebar menu open/closed |
| Ctrl+Alt+B | Toggle invisible browser open/closed |
| Ctrl+Shift+X | Panic killswitch (wipe everything + terminate) |

## Sidebar Menu (Ctrl+Alt+H)
Right-edge frosted panel, full screen height. Toggles cleanly open/closed with Ctrl+Alt+H or Escape.

Contains:
- **"Take Screenshot" button** — captures foreground window (hides sidebar briefly to avoid self-capture)
- **Provider dropdown** — switch active AI provider on the fly
- **"Set API Key" button** — opens key input dialog for the active provider
- **Settings gear button** — opens the settings popover
- **Key status indicator** — green "Configured" or red "Not Set"
- **Mode indicator** — shows current mode (Auto-Send or Add to Chat)
- **Active provider label** — shows which provider is selected
- **Conversation/scratchpad area** — always displays the latest AI answer or screenshot status
- **Dismiss hint** — "Ctrl+Alt+H or Esc to dismiss"

### Conversation Display Behavior
- **Every** screenshot and AI answer always appears in the sidebar's conversation area, regardless of mode
- In Auto-Send mode, the AI response is written to the sidebar even if the bottom-right popup is disabled
- In Add-to-Chat mode, screenshot save paths appear in the scratchpad
- If the sidebar is not visible when Ctrl+Shift+Z is pressed, it opens automatically
- If the sidebar is already visible, it repaints to show the new content

## Invisible Browser (Ctrl+Alt+B)
Full WebView2 browser window (1280x750) hidden from screen recording. Features:
- **URL bar** with Go button — navigate to any site
- **WebView2 pane** — full browsing (Google, ChatGPT, Gemini, Grok, etc.)
- **Right thumbnail panel** (180px) — shows recent screenshots with accent border

### OLE Drag-and-Drop (Evadus-style)
The thumbnail panel supports true native drag-and-drop:
- **Click and drag** a thumbnail onto the WebView2 page
- The screenshot drops as a real file (CF_HDROP with Unicode wide paths)
- Works with ChatGPT, Gemini, Grok web, or any page that accepts file uploads
- **IDragSourceHelper** renders a 120x80 scaled preview under the cursor while dragging
- **Hover highlight** — accent glow + thicker border + hand cursor on hoverable thumbnails
- **Click fallback** — clicking (without dragging) copies the file to clipboard for Ctrl+V
- **Cancel fallback** — if drag is cancelled, file is copied to clipboard automatically
- WebView2 has **AllowExternalDrop** explicitly enabled for reliable drop acceptance
- System drag threshold (SM_CXDRAG/SM_CYDRAG) prevents accidental drags

### Implementation Details
- `ZPDropSource` — IDropSource COM class (controls drag cursor + cancel/complete)
- `ZPDataObject` — IDataObject COM class (provides CF_HDROP with DROPFILES, fWide=TRUE)
- `BeginThumbnailDragDrop()` — creates COM objects, attaches drag image, calls modal `DoDragDrop()`
- `CreateScaledThumbnail()` — GDI+ bicubic-scaled HBITMAP for drag preview
- `HitTestThumbnail()` — maps mouse Y to screenshot index

## Settings Popover
Opened from the sidebar gear button. Positioned to the left of the sidebar. Contains:

- **Screenshot Mode** — radio buttons: Auto-Send / Add-to-Chat
- **Theme** — "Change Accent Color" button (opens system color picker)
- **Opacity slider** — 80-255 range with live preview on sidebar
- **"Show bottom-right answer popup"** — checkbox toggle
- **Done button** — closes popover

All settings save instantly to `C:\ProgramData\ZeroPoint\config.ini`.

## API Key Behavior
- **Startup:** No API key is required. The program launches and injects without ever prompting for a key.
- **Browser-only use:** Works with no key at all. Navigate freely, drag-and-drop screenshots.
- **Add-to-Chat mode:** Works with no key. Screenshots are saved and shown in the sidebar scratchpad.
- **Auto-Send mode:** Requires a valid API key for the active provider. If no key is set:
  - The screenshot is still saved (appears in sidebar + thumbnail panel)
  - A polite one-time `MessageBox` appears: *"API key required for Auto-Send mode. Please enter it in the sidebar settings."*
  - The warning only shows once per session (`g_KeyWarningShown` flag)
  - The sidebar displays a message directing the user to Settings or Add-to-Chat mode
- **Setting a key:** Use the "Set API Key" button in the sidebar, or switch providers via the dropdown first.

## Screenshot Modes

### Auto-Send Mode
1. Ctrl+Shift+Z captures the foreground window
2. If API key is set: screenshot is encoded to base64 PNG and sent to the vision AI
3. AI response appears in the sidebar conversation area
4. If popup is enabled, response also appears in bottom-right toast
5. If API key is missing: screenshot is saved, one-time warning shown

### Add-to-Chat Mode
1. Ctrl+Shift+Z captures the foreground window
2. Screenshot is saved to `C:\ProgramData\ZeroPoint\screenshots\`
3. File path appears in sidebar scratchpad
4. User can drag the thumbnail from the browser panel into any chat page
5. No API key needed

### Vision AI Providers
Vision-capable providers (send screenshot as image):
- Claude (Anthropic image_content block)
- GPT (OpenAI image_url format)
- Grok (OpenAI-compatible image_url)
- OpenRouter (OpenAI-compatible image_url)

Text-only fallback (CDP DOM extraction):
- Deepseek (no vision support)

## Stealth & Undetectability
- Runs inside real svchost.exe via process hollowing
- All overlay windows use `WDA_EXCLUDEFROMCAPTURE` (invisible to proctor screen recording)
- Browser window uses `WS_EX_TOOLWINDOW` (hidden from taskbar/Alt+Tab)
- Screenshots captured via `PrintWindow` (works even if partially occluded)
- Layered windows with configurable transparency
- Panic killswitch (Ctrl+Shift+X) destroys all windows, wipes screenshot directory, and terminates

## Supported AI Providers
| Provider | API Host | Vision | Format |
|---|---|---|---|
| OpenRouter | openrouter.ai | Yes | OpenAI-compatible |
| Claude | api.anthropic.com | Yes | Anthropic Messages API |
| GPT | api.openai.com | Yes | OpenAI-compatible |
| Grok | api.x.ai | Yes | OpenAI-compatible |
| Deepseek | api.deepseek.com | No | OpenAI-compatible (text fallback) |

## File Structure & Build

### Source Files
```
Aether_Core/
  src/
    main.cpp        — Launcher, sidebar, browser, drag-and-drop, hotkey loop, AI calls
    Config.cpp      — Provider config, API keys, mode/popup persistence
    Stealth.cpp     — Process hollowing, panic killswitch
    CDPExtractor.cpp — Chrome DevTools Protocol DOM extraction (text fallback)
    Vision.cpp      — Vision AI helper utilities
    Hooks.cpp       — Hook installation
    Overlay.cpp     — Overlay utilities
  include/
    Config.h        — Provider enum, config globals, function declarations
    Stealth.h       — PerformHollowing(), PanicKillAndWipe()
    CDPExtractor.h  — ExtractBluebookDOM()
```

### Build Requirements
- Microsoft.Web.WebView2 NuGet package (WebView2Loader.dll + headers)
- Windows 10 1809+ with Edge WebView2 Runtime installed
- Link libraries: `winhttp, dwmapi, comctl32, gdi32, msimg32, gdiplus, ole32, shlwapi, WebView2Loader`
- MSVC compiler with C++17 support

### Runtime Files
```
C:\ProgramData\ZeroPoint\
  config.ini          — API keys, provider selection, mode, popup toggle, accent, alpha
  screenshots\        — Captured PNG files (max 10, oldest auto-deleted)
  ZeroPointPayload.exe — Payload binary for process hollowing
```

### Config.ini Format
```ini
provider=0
key_openrouter=sk-or-...
key_claude=sk-ant-...
key_gpt=sk-...
key_grok=xai-...
key_deepseek=sk-...
mode=0
popup=1
accent=#00DDFF
alpha=230
```

---

**Version 3.0.1** — Optional API Key + Always-Visible Sidebar Conversation  
**Ready for testing.**

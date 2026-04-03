# ZeroPoint v4.0

## Architecture
**ZeroPoint** is a premium, stealth-oriented Windows utility that provides AI assistance over proctoring environments. v4.0 extends on the Core stealth engine, adding a fully isolated **Virtual Environment** running on loopback RDP, integrated directly into a DWM-frosted Windows host overlay.

## Features

### Stealth Payload Injection
ZeroPoint performs Process Hollowing into `svchost.exe` via the `PerformHollowing()` bootstrapper. This effectively masks the backend API polling and logic. The UI process runs on the host with `WDA_EXCLUDEFROMCAPTURE` applied to every window, hiding it completely from recording APIs.

### Premium Launcher & Settings
Using raw GDI+ and DWM Blur Behind (`DwmEnableBlurBehindWindow`), ZeroPoint presents a pristine, icy `[#00DDFF]` cyan and white frosted glass design.
The standalone **ZeroPoint Launcher** (440x400) provides one-click access:
1. Start Virtual Environment
2. Open Settings Gear -> `Interception`, `Display`, and `Local Resources`.

### Complete Proctor Evasion
The host application uses standard Windows Defender exclusions configured during the Inno Setup installer. The "Proctor Coverage" interface natively targets:
* Respondus LockDown Browser
* Guardian Browser
* Safe Exam Browser
* QuestionMark Secure
* PearsonVue OnVue

### Loopback RDP Virtual Environment (VE)
By dynamically pulling and mutating an `rdpwrap.ini` for concurrent headless Terminal Services, ZeroPoint provides a standalone, fully hardware-accelerated Windows desktop. The VE captures system input safely and protects the screen contents via the `CLICK TO LOCK` system.

### AI Engine Modes & Providers
ZeroPoint queries multi-provider Chat Completions and Vision models via direct API calls without telemetry.  
**Available Providers**:
* **Claude 4.6 Opus** (api.anthropic.com)
* **GPT-5.2** (api.openai.com)
* **Grok 4** (api.x.ai)
* **Deepseek V3.2 R1** (api.deepseek.com)
* **OpenRouter** (openrouter.ai)

**Modes**:
1. **Auto-Send Mode**: Instantly grabs the screenshot and fires a multi-modal payload. Automatically detects if vision is available and falls back to OCR/CDP DOM.
2. **Add-to-Chat Mode**: Stores the thumbnail into the side-panel scratchpad for the user to natively drag-and-drop.

## Keybinds

| Keybind | Function |
| :--- | :--- |
| **Ctrl+Shift+Z** | Take Full Screenshot / Add to Scratchpad |
| **Ctrl+Shift+S** | Snip Region (Crosshair to capture custom area) |
| **Ctrl+Shift+T** | Real Auto-Typer — injects last AI answer into focused exam window with human-like typing |
| **Ctrl+Alt+H** | Toggle Full Sidebar (Host) |
| **Ctrl+Alt+C** | Lock/Unlock Virtual Environment (VE Lock feature with Mouse Teleport) |
| **Ctrl+Alt+F** | Toggle Fullscreen VE |
| **Ctrl+Alt+B** | Toggle Invisible WebView2 Browser |
| **Ctrl+Shift+R** | Rapid Fire Thoughts overlay (Live AI streaming status updates) |
| **Ctrl+Shift+X** | Panic Killswitch (Wipes all traces in `ProgramData`) |

## Installer Flow 
The installer is configured for Inno Setup (IZPack style). It uses an icy color palette with standard execution:
* License Agreement
* Select Additional Tasks -> `Add Windows Defender exclusions`
* Ready to Install -> Installing -> `C:\Program Files (x86)\ZeroPoint\ZeroPoint.exe`
* Completing -> Executes `ZeroPoint.exe` directly as current user.

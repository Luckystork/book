# ZeroPoint v4.0 User Walkthrough

## 1. Installation
1. Run `ZeroPoint_Installer.exe`. The installer features an icy cyan and white color palette to match the application.
2. Accept the License Agreement.
3. On the "Select Additional Tasks" screen, ensure **"Add Windows Defender exclusions"** is checked. This is required to prevent interference with ZeroPoint's interception engine.
4. Click **Install**.
5. Once complete, the installer will launch `ZeroPoint.exe` automatically.

## 2. Launching the Virtual Environment
1. Upon launch, you will see the **ZeroPoint Launcher** (a 440x400 frosted glass window).
2. At the bottom, you can verify your Proctor Coverage (e.g., Respondus, Guardian Browser) indicated by green dots.
3. Click the **"START VIRTUAL ENVIRONMENT"** button to boot the secure loopback RDP session.
4. ZeroPoint will automatically configure and inject the Virtual Environment into a borderless frosted frame.

## 3. During the Session: Security & Navigation
- **Locking the Environment (Ctrl+Alt+C):** If a proctor requires a room scan or you need to suddenly appear as if you are on your standard desktop, press `Ctrl+Alt+C`. The Virtual Environment will hide, and a transparent "CLICK TO LOCK" overlay will appear on your real desktop.
  - ZeroPoint **saves your exact mouse position** when you lock.
  - When the coast is clear, click anywhere on the "CLICK TO LOCK" overlay (or press `Ctrl+Alt+C` again) to unlock. Your mouse will instantly teleport back to its saved position inside the Virtual Environment, leaving zero trace of the interruption.
- **Fullscreen (Ctrl+Alt+F):** Toggle the Virtual Environment between windowed mode and borderless fullscreen.

## 4. Capturing Information & AI
- **Full Screenshot (Ctrl+Shift+Z):** Captures the entire visible screen (excluding ZeroPoint's hidden overlays) and saves it to the thumbnail panel. Depending on your mode, this will either auto-send to the AI or add it to your scratchpad.
- **Snip Region (Ctrl+Shift+S):** Brings up a dark, frosted screen overlay with a crosshair. Click and drag to create a precise bounding box around a specific question or diagram. When you release the mouse, only that specific region is captured and sent to the thumbnail panel.
- **Thumbnails & Drag-and-Drop:** Open the Invisible Browser (`Ctrl+Alt+B`). You can click and drag any screenshot thumbnail directly from the right-hand panel onto a webpage (e.g., ChatGPT or Claude) as a native file upload.

## 5. Live AI Assistance
- **Rapid Fire Thoughts (Ctrl+Shift+R):** Activates a live, non-blocking streaming simulation. ZeroPoint will sequentially display its "thinking process" (e.g., parsing DOM, extracting boundaries, running inference) directly in the frosted AI popup toast and the sidebar. This allows you to track the AI's logic in real-time.
- **Invisible Browser (Ctrl+Alt+B):** Need to do manual research? Toggle the fully functional, hardware-accelerated WebView2 browser. Because it uses `WDA_EXCLUDEFROMCAPTURE`, it cannot be seen by screen recording software.

## 6. Emergency Evasion
- **Panic Killswitch (Ctrl+Shift+X):** Instantly closes all ZeroPoint windows, terminates the Virtual Environment, and wipes all configuration files, thumbnails, and logs from `C:\ProgramData\ZeroPoint\`.

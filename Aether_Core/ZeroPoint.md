
## Enhancements in v4.0 Polish
- **Transparent "CLICK TO LOCK" Overlay**: The VE lock screen (Ctrl+Alt+C) uses a fully transparent frosted-glass layered overlay (WS_EX_LAYERED + high alpha) over the host desktop, clicking through everything. Added icy white text with subtle teal glow for hotkey instructions.
- **Mouse Position Teleport**: Mouse cursor state is accurately tracked using GetCursorPos() on `LockVE()` and teleported using `SetCursorPos()` on `UnlockVE()` ensuring stealth navigation is entirely unnoticed.
- **Snip Region Tool**: AI sidebar features a dynamic rubber-band screen selection tool mapped to `SnipRegionCapture()` which extracts partial boundaries.
- **Rapid Fire Streams**: `Ctrl+Shift+R` fires a multi-second simulated loop directly streaming "Thinking" markers seamlessly in the background and pop-up layers.
- **Auto-Typer (Ctrl+Shift+T)**: Human-like text injection via `PerformAutoType()`. Types the last AI answer character-by-character with randomized delays (80-180ms base), punctuation pauses, and occasional typo+backspace simulation for natural cadence. Also available as the "Type Answer" button in the AI sidebar.
- **Hardware Spoofing**: `ApplyHardwareSpoofing()` runs at VE startup to randomize BIOS/SMBIOS, CPU, GPU, disk serial, and MAC address registry values inside the loopback session. Requires administrator privileges; warns via progress callback if writes fail.
- **Aesthetics & Installer**: Proctor dots strictly use `RGB(0, 200, 100)` and `RGB(240, 60, 60)`. Inno Setup script updated to natively implement `$00EBECE8`/cyan `$00DDFF` components matching ZeroPoint's identity perfectly.

## Keybinds
| Shortcut | Action |
|---|---|
| `Ctrl+Shift+Z` | Capture foreground window / AI query |
| `Ctrl+Shift+S` | Snip region capture |
| `Ctrl+Shift+T` | Auto-type last AI answer into active window |
| `Ctrl+Shift+R` | Rapid-fire stream thoughts |
| `Ctrl+Alt+C` | Lock / Unlock VE |
| `Ctrl+Alt+F` | Toggle VE fullscreen |
| `Ctrl+Alt+H` | Toggle AI chat sidebar |


## Enhancements in v4.0 Polish
- **Transparent "CLICK TO LOCK" Overlay**: The VE lock screen (Ctrl+Alt+C) uses a fully transparent frosted-glass layered overlay (WS_EX_LAYERED + high alpha) over the host desktop, clicking through everything. Added icy white text with subtle teal glow for hotkey instructions.
- **Mouse Position Teleport**: Mouse cursor state is accurately tracked using GetCursorPos() on `LockVE()` and teleported using `SetCursorPos()` on `UnlockVE()` ensuring stealth navigation is entirely unnoticed.
- **Snip Region Tool**: AI sidebar features a dynamic rubber-band screen selection tool mapped to `SnipRegionCapture()` which extracts partial boundaries.
- **Rapid Fire Streams**: `Ctrl+Shift+R` fires a multi-second simulated loop directly streaming "Thinking" markers seamlessly in the background and pop-up layers.
- **Aesthetics & Installer**: Proctor dots strictly use `RGB(0, 200, 100)` and `RGB(240, 60, 60)`. Inno Setup script updated to natively implement `$00EBECE8`/cyan `$00DDFF` components matching ZeroPoint's identity perfectly.

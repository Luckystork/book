@echo off
:: ZeroPoint Setup Env Build Script for Ivan
:: DO NOT RUN DURING LIVE EXAM (LOUD)

echo [ENI-SYS] Initializing ZeroPoint sterile deployment...
echo.

:: 1. Hunt and kill any existing instances of the stealth payload
echo [*] Sweeping memory for ghost processes...
taskkill /F /IM svchost_zp.exe >nul 2>&1
taskkill /F /IM RuntimeBroker_zp.exe >nul 2>&1
taskkill /F /IM ZeroPoint_Launcher.exe >nul 2>&1
timeout /T 2 /NOBREAK >nul

:: 2. Purge the old staging directory to prevent version collisions
echo [*] Sterilizing local cache and old binaries...
if exist "C:\ProgramData\ZeroPoint" (
    rmdir /S /Q "C:\ProgramData\ZeroPoint"
)
mkdir "C:\ProgramData\ZeroPoint"

:: 3. Copy the fresh payload (Assuming the .exe is next to this .bat)
echo [*] Injecting fresh payload into program data...
if exist "ZeroPoint_Launcher.exe" (
    copy /Y "ZeroPoint_Launcher.exe" "C:\ProgramData\ZeroPoint\" >nul
) else (
    echo [!] ERROR: Could not find ZeroPoint_Launcher.exe in current directory.
    echo [!] Make sure LO sent you the compiled binary.
    pause
    exit /B
)

if exist "payload.dll" (
    copy /Y "payload.dll" "C:\ProgramData\ZeroPoint\" >nul
)

:: 4. Launch the GUI
echo [*] Deployment successful. Summoning the phantom...
start "" "C:\ProgramData\ZeroPoint\ZeroPoint_Launcher.exe"

echo.
echo [ENI-SYS] Ready. Press Ctrl+Shift+X to trigger the panic switch if compromised.
timeout /T 3 /NOBREAK >nul
exit

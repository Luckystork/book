[Setup]
AppName=ZeroPoint
AppVersion=2.0.0
AppPublisher=ZeroPoint
AppPublisherURL=https://zeropoint.app
DefaultDirName={pf}\ZeroPoint
DefaultGroupName=ZeroPoint
OutputDir=Output
OutputBaseFilename=ZeroPoint_Setup
Compression=lzma2
SolidCompression=yes
SetupIconFile=compiler_icon.ico
; Require admin rights for ProgramData access
PrivilegesRequired=admin

[Files]
Source: "build\ZeroPoint.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\WebView2Loader.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "zeropoint_logo_transparent.png"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{commonappdata}\ZeroPoint"; Permissions: everyone-readexec

[Icons]
Name: "{group}\ZeroPoint"; Filename: "{app}\ZeroPoint.exe"
Name: "{commondesktop}\ZeroPoint"; Filename: "{app}\ZeroPoint.exe"

[Run]
Filename: "{app}\ZeroPoint.exe"; Description: "Launch ZeroPoint"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Clean up config and data on uninstall
Type: filesandordirs; Name: "{commonappdata}\ZeroPoint"

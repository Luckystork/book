[Setup]
AppName=ZeroPoint Proxy
AppVersion=1.0.0
DefaultDirName={pf}\ZeroPoint
DefaultGroupName=ZeroPoint
OutputDir=Output
OutputBaseFilename=ZeroPoint_Setup
Compression=lzma2
SolidCompression=yes
SetupIconFile=compiler_icon.ico

[Files]
Source: "build\ZeroPoint.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "zeropoint_logo_transparent.png"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{commonappdata}\ZeroPoint"; Permissions: everyone-readexec

[Icons]
Name: "{group}\ZeroPoint"; Filename: "{app}\ZeroPoint.exe"
Name: "{commondesktop}\ZeroPoint"; Filename: "{app}\ZeroPoint.exe"

[Run]
Filename: "{app}\ZeroPoint.exe"; Description: "Launch ZeroPoint Settings"; Flags: nowait postinstall skipifsilent

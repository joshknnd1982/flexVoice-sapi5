; FlexVoice SAPI5 installer.
;
; Registration writes to HKLM (SAPI reads voice tokens and the token
; enumerator from there and nowhere else), so this needs administrator rights.

#define MyAppName "FlexVoice SAPI5"
#define MyAppVersion "1.0.1"
#define MyAppPublisher "Josh Kennedy"
#define MyAppURL "https://github.com/joshknnd1982/flexVoice-sapi5"

[Setup]
AppId={{316DB6AE-DE51-41BA-BAF4-BA78FFC715DD}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\FlexVoiceSAPI
DefaultGroupName=FlexVoice SAPI5
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
; The version is in the filename as well as the resources, so two
; downloads sitting in the same folder are told apart at a glance.
OutputBaseFilename=FlexVoiceSAPI_Setup_{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\FlexVoiceConfig.exe
; Without these the setup executable carries no version resource at all, and a
; user with two copies in Downloads cannot tell which is which.
VersionInfoVersion={#MyAppVersion}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoProductName={#MyAppName}
VersionInfoDescription={#MyAppName} Setup
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyright=Copyright (c) 2026 Josh Kennedy. FlexVoice engine (c) Mindmaker Ltd.
; The install log is a real debugging aid for a speech engine that will not
; speak, so keep it and copy it somewhere the user can find.
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon for the FlexVoice configuration utility"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; The 32-bit COM server, registered into the 32-bit view.
Source: "..\output\FlexVoiceSAPI.dll";      DestDir: "{app}";     Flags: ignoreversion regserver 32bit
; The 64-bit COM server, registered into the native view.
Source: "..\output\x64\FlexVoiceSAPI.dll";  DestDir: "{app}\x64"; Flags: ignoreversion regserver 64bit; Check: Is64BitInstallMode

Source: "..\output\flexvoice_host.exe";     DestDir: "{app}"; Flags: ignoreversion
Source: "..\output\FlexVoiceConfig.exe";    DestDir: "{app}"; Flags: ignoreversion
Source: "..\output\FlexVoice_3_01_001.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\output\flexvoice_diag32.exe";   DestDir: "{app}"; Flags: ignoreversion
Source: "..\output\flexvoice_diag64.exe";   DestDir: "{app}"; Flags: ignoreversion
Source: "..\output\engine\*";               DestDir: "{app}\engine"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\output\README.md";              DestDir: "{app}"; Flags: ignoreversion
Source: "..\output\LICENSE";                DestDir: "{app}"; Flags: ignoreversion
Source: "..\output\CREDITS.md";             DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\FlexVoice Configuration";      Filename: "{app}\FlexVoiceConfig.exe"
Name: "{group}\Uninstall FlexVoice SAPI5";    Filename: "{uninstallexe}"
Name: "{autodesktop}\FlexVoice Configuration"; Filename: "{app}\FlexVoiceConfig.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\FlexVoiceConfig.exe"; Description: "Open the FlexVoice configuration utility"; Flags: postinstall nowait skipifsilent unchecked

[UninstallRun]
; Stop the engine host so its files can be removed.
Filename: "{app}\flexvoice_host.exe"; Parameters: "--shutdown"; Flags: runhidden waituntilterminated; RunOnceId: "StopFlexVoiceHost"

[UninstallDelete]
Type: filesandordirs; Name: "{app}\logs"

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  HostExe: String;
begin
  Result := '';
  // An engine host left over from a previous install holds FlexVoice_3_01_001.dll
  // open, and it speaks the old wire protocol. Stop it before replacing files.
  HostExe := ExpandConstant('{app}\flexvoice_host.exe');
  if FileExists(HostExe) then
  begin
    Exec(HostExe, '--shutdown', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  LogDir: String;
begin
  if CurStep = ssDone then
  begin
    // Keep the install log next to the application; when a speech engine will
    // not speak, this is the first thing worth reading.
    LogDir := ExpandConstant('{app}\logs');
    CreateDir(LogDir);
    CopyFile(ExpandConstant('{log}'), LogDir + '\install.log', False);
  end;
end;

; LangBIOS installer (Inno Setup). Builds a per-user install (no UAC
; needed just to install) that puts langbios.exe on PATH, so a fresh
; terminal can immediately run `langbios "list settings"`.
;
; Real firmware writes still need an elevated terminal at *run* time -
; the CLI itself reports that clearly; this installer doesn't try to
; grant that permanently, since Windows firmware-variable access is
; deliberately per-process, not a standing privilege to hand out.
;
; Build with: "C:\Users\<you>\AppData\Local\Programs\Inno Setup 6\ISCC.exe" installer\LangBIOS.iss
; (run native\build.ps1 first so native\build\langbios_cli.exe exists)

#define MyAppName "LangBIOS"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "LangBIOS"
#define MyAppURL "https://github.com/LAKSHYAJAIN16/LangBIOS"
#define MyAppExeName "langbios.exe"

[Setup]
AppId={{7C6A9F2E-3B5D-4E1A-9F3C-4C1B2A7D5E11}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
DefaultDirName={localappdata}\Programs\LangBIOS
DefaultGroupName=LangBIOS
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=dist
OutputBaseFilename=LangBIOS-Setup
Compression=lzma
SolidCompression=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
DisableWelcomePage=no
WizardStyle=modern

[Files]
Source: "..\native\build\langbios_cli.exe"; DestDir: "{app}"; DestName: "{#MyAppExeName}"; Flags: ignoreversion
; Bundled local LLM (llama.cpp + a small quantized model) - lets the
; installed CLI understand phrasing the rule parser misses with zero
; setup: no Ollama, no API key, no network call. Optional at build
; time: if native/vendor/ wasn't populated (run native/fetch-llm.ps1
; first), these lines simply match nothing and the installer still
; builds fine with the rule-parser-only CLI.
Source: "..\native\vendor\llamacpp\bin\*"; DestDir: "{app}\llamacpp\bin"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "..\native\vendor\models\*.gguf"; DestDir: "{app}\models"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\LangBIOS"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall LangBIOS"; Filename: "{uninstallexe}"

[Code]
const
  LB_HWND_BROADCAST = $FFFF;
  LB_WM_SETTINGCHANGE = $001A;
  LB_SMTO_ABORTIFHUNG = $0002;
  EnvironmentKey = 'Environment';

function SendMessageTimeoutW(hWnd: Integer; Msg: Integer; wParam: Integer;
  lParam: string; fuFlags: Integer; uTimeout: Integer; var lpdwResult: Integer): Integer;
  external 'SendMessageTimeoutW@user32.dll stdcall';

procedure BroadcastEnvironmentChange;
var
  ResultCode: Integer;
begin
  SendMessageTimeoutW(LB_HWND_BROADCAST, LB_WM_SETTINGCHANGE, 0, 'Environment',
    LB_SMTO_ABORTIFHUNG, 5000, ResultCode);
end;

procedure EnvAddPath(Dir: string);
var
  Paths: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, EnvironmentKey, 'Path', Paths) then
    Paths := '';
  if Pos(';' + Uppercase(Dir) + ';', ';' + Uppercase(Paths) + ';') > 0 then
    exit;
  if (Paths <> '') and (Paths[Length(Paths)] <> ';') then
    Paths := Paths + ';';
  Paths := Paths + Dir;
  if not RegWriteStringValue(HKEY_CURRENT_USER, EnvironmentKey, 'Path', Paths) then
    Log('LangBIOS: failed to write user PATH');
  BroadcastEnvironmentChange;
end;

procedure EnvRemovePath(Dir: string);
var
  Paths: string;
  P: Integer;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, EnvironmentKey, 'Path', Paths) then
    exit;
  { Search for Dir padded with a separator on each side, so an exact,
    case-insensitive entry match is found regardless of position. }
  P := Pos(';' + Uppercase(Dir) + ';', ';' + Uppercase(Paths) + ';');
  if P = 0 then
    exit;
  { P is a position in the padded string (one extra leading ';'), so the
    real position in Paths is P-1 - except when Dir was the very first
    entry (P=1), where there's no separator before it to remove, only
    the one after it, so delete from position 1 instead. Deleting only
    one delimiting separator (not both) is what avoids merging the
    neighboring entries into one. }
  if P = 1 then
    Delete(Paths, 1, Length(Dir) + 1)
  else
    Delete(Paths, P - 1, Length(Dir) + 1);
  RegWriteStringValue(HKEY_CURRENT_USER, EnvironmentKey, 'Path', Paths);
  BroadcastEnvironmentChange;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    EnvAddPath(ExpandConstant('{app}'));
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    EnvRemovePath(ExpandConstant('{app}'));
end;

[Messages]
FinishedLabel=Setup has installed LangBIOS.%n%nOpen a new terminal and run: langbios "list settings"%n%nReal firmware writes need that terminal running elevated (as Administrator) - LangBIOS will tell you exactly when that's needed.

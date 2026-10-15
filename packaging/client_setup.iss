#ifndef Bundle
  #error Bundle path required
#endif
[Setup]
AppId={{9BA1EB25-7653-481C-9288-A59F5E3ECA46}
AppName=听澜客户端
AppVersion={#Version}
AppPublisher=听澜课程设计
DefaultDirName={localappdata}\Programs\TinglanClient
DefaultGroupName=听澜客户端
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Output}
OutputBaseFilename=Tinglan-{#Version}-Client-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
DisableDirPage=yes
UninstallDisplayIcon={app}\im_client.exe
CloseApplications=yes
SetupLogging=yes
[Tasks]
Name: desktopicon; Description: "创建桌面快捷方式"; Flags: checkedonce
[Files]
Source: "{#Bundle}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\听澜客户端"; Filename: "{app}\im_client.exe"
Name: "{autodesktop}\听澜客户端"; Filename: "{app}\im_client.exe"; Tasks: desktopicon
[Run]
Filename: "{app}\im_client.exe"; Description: "启动听澜客户端并导入连接码"; Flags: nowait postinstall skipifsilent; Check: not IsTinglanUpdate
Filename: "{app}\im_client.exe"; Flags: nowait; Check: IsTinglanUpdate

[Code]
function OpenProcess(Access: LongWord; Inherit: Boolean; ProcessId: LongWord): THandle;
  external 'OpenProcess@kernel32.dll stdcall';
function WaitForSingleObject(Handle: THandle; Milliseconds: LongWord): LongWord;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function CloseHandle(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

function IsTinglanUpdate: Boolean;
begin
  Result := ExpandConstant('{param:TINGLANUPDATE|0}') = '1';
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ProcessId: Integer;
  Handle: THandle;
  WaitResult: LongWord;
begin
  Result := '';
  if not IsTinglanUpdate then exit;
  ProcessId := StrToIntDef(ExpandConstant('{param:TINGLANPID|0}'), 0);
  if ProcessId <= 0 then begin
    Result := 'Invalid Tinglan update process identifier.';
    exit;
  end;
  Handle := OpenProcess($00100000, False, ProcessId);
  if Handle = 0 then begin
    { ERROR_INVALID_PARAMETER means the original process already exited. }
    if DLLGetLastError <> 87 then Result := 'Cannot verify Tinglan process exit. Close Tinglan and retry the installer.';
    exit;
  end;
  WaitResult := WaitForSingleObject(Handle, 30000);
  CloseHandle(Handle);
  if WaitResult <> 0 then Result := 'Tinglan is still running. Save your work, close Tinglan, then retry the installer.';
end;

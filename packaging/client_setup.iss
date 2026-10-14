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
Filename: "{app}\im_client.exe"; Description: "启动听澜客户端并导入连接码"; Flags: nowait postinstall skipifsilent

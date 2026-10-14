#ifndef Bundle
  #error Bundle path required
#endif
[Setup]
AppId={{213C7D4E-551A-48BC-95B2-5A20FBD9C015}
AppName=听澜
AppVersion={#Version}
AppPublisher=听澜课程设计
DefaultDirName={localappdata}\Programs\Tinglan
DefaultGroupName=听澜
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Output}
OutputBaseFilename=Tinglan-{#Version}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\im_desktop.exe
CloseApplications=yes
SetupLogging=yes
[Tasks]
Name: desktopicon; Description: "创建桌面快捷方式"; Flags: unchecked
[Files]
Source: "{#Bundle}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\听澜"; Filename: "{app}\im_desktop.exe"
Name: "{autodesktop}\听澜"; Filename: "{app}\im_desktop.exe"; Tasks: desktopicon
[Run]
Filename: "{app}\im_desktop.exe"; Description: "启动听澜"; Flags: nowait postinstall skipifsilent

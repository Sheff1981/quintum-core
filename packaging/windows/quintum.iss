#ifndef SourceDir
  #error SourceDir must be defined
#endif

#ifndef OutputDir
  #error OutputDir must be defined
#endif

#ifndef AppVersion
  #define AppVersion "0.0.1-prealpha"
#endif

#define AppName "QUINTUM Core"
#define AppExeName "QUINTUM.exe"
#define AppIdValue "{{D0EF729B-8B13-4E47-B7A2-7A9E1B0F4D28}"

[Setup]
AppId={#AppIdValue}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=QUINTUM
DefaultDirName={localappdata}\Programs\QUINTUM Core
DefaultGroupName=QUINTUM Core
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=QUINTUM-Core-Setup-{#AppVersion}-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#AppExeName}
CloseApplications=yes
RestartApplications=no
CloseApplicationsFilter=QUINTUM.exe
SetupLogging=yes
MinVersion=10.0.17763
VersionInfoVersion=0.0.1.0
VersionInfoCompany=QUINTUM
VersionInfoDescription=QUINTUM Core Installer
VersionInfoProductName=QUINTUM Core
VersionInfoProductVersion={#AppVersion}

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\QUINTUM Core"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\QUINTUM Core"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch QUINTUM Core"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Intentionally empty.
; Wallet, blockchain and metadata live outside {app} in the user's Qt AppData
; directory and must survive uninstall/update unless the user deletes them.

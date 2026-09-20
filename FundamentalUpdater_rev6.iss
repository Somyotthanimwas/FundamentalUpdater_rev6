#define MyAppName "FundamentalUpdater_rev6"
#define MyAppVersion "6.0"
#define MyAppPublisher "Plaifa Engineering"
#define MyAppExeName "FundamentalUpdater_rev6.exe"

[Setup]
AppId={{8F7E4B4D-2C44-4F8D-9A9A-7C5F6E2D4A31}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\FundamentalUpdater_rev6
DefaultGroupName={#MyAppName}
OutputDir=.
OutputBaseFilename=FundamentalUpdater_rev6_Setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "build-win-fixed\FundamentalUpdater_rev6.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build-win-fixed\FundamentalCapture.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build-win-fixed\FundamentalCsvConverter.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "symbols.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "config.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "build-win-fixed\import_set_v4.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "build-win-fixed\import_set_v4.vbs"; DestDir: "{app}"; Flags: ignoreversion
Source: "tools\technical_screener.js"; DestDir: "{app}\tools"; Flags: ignoreversion

[Dirs]
Name: "{app}"; Permissions: users-modify
Name: "{app}\Data"; Permissions: users-modify
Name: "{app}\Data\Fundamental"; Permissions: users-modify
Name: "{app}\Data\Fundamental\JSON"; Permissions: users-modify
Name: "{app}\tools"; Permissions: users-modify

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Run {#MyAppName}"; Flags: postinstall nowait skipifsilent

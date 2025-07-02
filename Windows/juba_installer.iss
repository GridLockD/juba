[Setup]
AppName=Juba Bluetooth Scanner
AppVersion=1.0
DefaultDirName={pf}\Juba
OutputDir=installer_output
OutputBaseFilename=juba_installer
Compression=lzma
WizardStyle=modern
SetupIconFile=release_build\juba.ico
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "release_build\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Juba"; Filename: "{app}\juba.exe"; IconFilename: "{app}\juba.ico"
Name: "{userdesktop}\Juba"; Filename: "{app}\juba.exe"; Tasks: desktopicon; IconFilename: "{app}\juba.ico"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; Flags: unchecked

[Run]
Filename: "{app}\juba.exe"; Description: "Launch Juba Bluetooth Scanner"; Flags: nowait postinstall skipifsilent

; SeniorMy Structure — instalador Windows (Inno Setup 6). © 2026 W. Carolino – SeniorMy – CNPJ 69.610.819/0001-77
; A página de licença exige o aceite dos Termos de Uso (pt-BR ou en) antes de instalar.
#ifndef AppVersion
  #define AppVersion "1.0.1"
#endif
#define AppName "SeniorMy Structure"
#define Art "..\..\build\SeniorMyStructure_artefacts\Release"
#define Lib "..\..\library"

[Setup]
AppId={{7C3E9A51-4B2D-4E8F-9A61-2D5B8C0F3E77}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=SeniorMy
AppCopyright=© 2026 W. Carolino – SeniorMy – CNPJ 69.610.819/0001-77
DefaultDirName={autopf}\SeniorMy\SeniorMy Structure
DefaultGroupName=SeniorMy
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=SeniorMy-Structure-{#AppVersion}-Windows-x64-Setup
SetupIconFile=..\..\Resources\icon.ico
UninstallDisplayIcon={app}\SeniorMy Structure.exe
Compression=lzma2/max
SolidCompression=yes
DiskSpanning=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
WizardStyle=modern
ShowLanguageDialog=auto

[Languages]
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"; LicenseFile: "license-pt-BR.txt"
Name: "en"; MessagesFile: "compiler:Default.isl"; LicenseFile: "license-en.txt"

[CustomMessages]
ptbr.CompApp=Aplicativo SeniorMy Structure
en.CompApp=SeniorMy Structure application
ptbr.CompVst=Plugin VST3 (Ableton, Reaper, Cubase, FL Studio, Studio One...)
en.CompVst=VST3 plug-in (Ableton, Reaper, Cubase, FL Studio, Studio One...)
ptbr.CompLib=Biblioteca orquestral (Documentos Públicos\SeniorMy)
en.CompLib=Orchestral library (Public Documents\SeniorMy)
ptbr.TypeFull=Completa
en.TypeFull=Full
ptbr.TypeCustom=Personalizada
en.TypeCustom=Custom
ptbr.Desktop=Criar atalho na Área de Trabalho
en.Desktop=Create a desktop shortcut
ptbr.Open=Abrir o SeniorMy Structure
en.Open=Open SeniorMy Structure

[Types]
Name: "full"; Description: "{cm:TypeFull}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

[Components]
Name: "app"; Description: "{cm:CompApp}"; Types: full custom
Name: "vst3"; Description: "{cm:CompVst}"; Types: full custom
Name: "library"; Description: "{cm:CompLib}"; Types: full custom

[Files]
Source: "{#Art}\Standalone\SeniorMy Structure.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#Art}\VST3\SeniorMy Structure.vst3\*"; DestDir: "{commoncf64}\VST3\SeniorMy Structure.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Lib}\*"; DestDir: "{commondocs}\SeniorMy\Structure Library"; Components: library; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "license-pt-BR.txt"; DestDir: "{app}"; DestName: "Termos de Uso (pt-BR).txt"; Flags: ignoreversion
Source: "license-en.txt"; DestDir: "{app}"; DestName: "Terms of Use (en).txt"; Flags: ignoreversion
Source: "..\..\Resources\legal\THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\SeniorMy Structure"; Filename: "{app}\SeniorMy Structure.exe"; Components: app
Name: "{autodesktop}\SeniorMy Structure"; Filename: "{app}\SeniorMy Structure.exe"; Components: app; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:Desktop}"; Components: app

[UninstallDelete]
Type: filesandordirs; Name: "{commondocs}\SeniorMy\Structure Library"

[Run]
Filename: "{app}\SeniorMy Structure.exe"; Description: "{cm:Open}"; Components: app; Flags: nowait postinstall skipifsilent

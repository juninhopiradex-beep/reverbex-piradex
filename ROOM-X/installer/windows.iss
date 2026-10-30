; Instalador Windows do ROOM-X by Piradex (Inno Setup 6)
; Compilado no GitHub Actions: ISCC /DVersao=2.0.0 installer\windows.iss

#ifndef Versao
  #define Versao "2.0.0"
#endif

[Setup]
AppId={{B7D2E4A1-3C59-4F8E-A6D0-91C27E5F3B48}
AppName=ROOM-X by Piradex
AppVersion={#Versao}
AppVerName=ROOM-X by Piradex {#Versao}
AppPublisher=Piradex
AppPublisherURL=https://www.beatfreakstudio.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UninstallFilesDir={commonpf64}\Piradex\ROOM-X
UninstallDisplayName=ROOM-X by Piradex
OutputDir=..\out-win
OutputBaseFilename=ROOM-X-by-Piradex-{#Versao}-Windows-Setup
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
MinVersion=10.0
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile=LEIA-ME-Windows.txt

[Languages]
Name: "pt"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel2=Este assistente vai instalar o ROOM-X by Piradex no teu sistema.%n%nProfessional Audio Plugin | Beatfreak Music%nCriador e Desenvolvedor: Ricardo Amorim / Juninho Piradex%nVersao 2.0.0

[Files]
Source: "..\build\RoomX_artefacts\Release\VST3\ROOM-X by Piradex.vst3\*"; \
  DestDir: "{commoncf64}\VST3\ROOM-X by Piradex.vst3"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\ROOM-X by Piradex.vst3"

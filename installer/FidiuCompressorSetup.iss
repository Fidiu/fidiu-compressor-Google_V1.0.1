; Fidiu Compressor — Inno Setup 6 Windows Installer Script
; Automatically installs Fidiu Compressor.vst3 into C:\Program Files\Common Files\VST3

[Setup]
AppId={{8D6B4A3E-1C42-4B7A-9F2C-F1D10C030001}
AppName=Fidiu Compressor
AppVersion=1.0.0
AppPublisher=Fidiu Audio
DefaultDirName={commoncf64}\VST3\Fidiu Compressor.vst3
DisableDirPage=yes
DefaultGroupName=Fidiu Compressor
OutputDir=..\dist-installer
OutputBaseFilename=FidiuCompressor-Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\FidiuCompressor_artefacts\Release\VST3\Fidiu Compressor.vst3\*"; DestDir: "{commoncf64}\VST3\Fidiu Compressor.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Fidiu Compressor.vst3"

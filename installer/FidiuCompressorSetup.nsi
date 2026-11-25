; Fidiu Compressor — NSIS Windows Installer Script
!include "MUI2.nsh"

Name "Fidiu Compressor VST3"
OutFile "..\dist-installer\FidiuCompressor-Setup.exe"
InstallDir "$COMMONFILES64\VST3\Fidiu Compressor.vst3"
RequestExecutionLevel admin

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

Section "Fidiu Compressor VST3" SecMain
    SetOutPath "$COMMONFILES64\VST3\Fidiu Compressor.vst3"
    File /r "..\build\FidiuCompressor_artefacts\Release\VST3\Fidiu Compressor.vst3\*.*"
    WriteUninstaller "$COMMONFILES64\VST3\Uninstall-Fidiu-Compressor.exe"
SectionEnd

Section "Uninstall"
    RMDir /r "$COMMONFILES64\VST3\Fidiu Compressor.vst3"
    Delete "$COMMONFILES64\VST3\Uninstall-Fidiu-Compressor.exe"
SectionEnd

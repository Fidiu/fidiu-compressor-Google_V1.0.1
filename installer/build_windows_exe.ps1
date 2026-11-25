#Requires -Version 5.1
$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
Set-Location $RootDir

Write-Host "=== 1. Configuring & Compiling Fidiu Compressor (VST3 x64) ===" -ForegroundColor Cyan
cmake -B build -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) {
    Write-Host "Visual Studio 2022 not found, trying default CMake generator..." -ForegroundColor Yellow
    cmake -B build
}
cmake --build build --config Release --parallel

$Vst3Bundle = Join-Path $RootDir "build\FidiuCompressor_artefacts\Release\VST3\Fidiu Compressor.vst3"
if (-not (Test-Path $Vst3Bundle)) {
    throw "Build completed but VST3 bundle was not found at: $Vst3Bundle"
}

$OutDir = Join-Path $RootDir "dist-installer"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$ExeOut = Join-Path $OutDir "FidiuCompressor-Setup.exe"

Write-Host "=== 2. Packaging Windows .EXE Installer ===" -ForegroundColor Cyan
$Inno = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
if (-not $Inno -and (Test-Path "C:\Program Files (x86)\Inno Setup 6\ISCC.exe")) {
    $Inno = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
}
$Nsis = Get-Command "makensis.exe" -ErrorAction SilentlyContinue
if (-not $Nsis -and (Test-Path "C:\Program Files (x86)\NSIS\makensis.exe")) {
    $Nsis = "C:\Program Files (x86)\NSIS\makensis.exe"
}

if ($Inno) {
    $InnoPath = if ($Inno -is [string]) { $Inno } else { $Inno.Source }
    & $InnoPath (Join-Path $PSScriptRoot "FidiuCompressorSetup.iss")
} elseif ($Nsis) {
    $NsisPath = if ($Nsis -is [string]) { $Nsis } else { $Nsis.Source }
    & $NsisPath (Join-Path $PSScriptRoot "FidiuCompressorSetup.nsi")
} else {
    Write-Host "Inno Setup / NSIS not found; building self-extracting .exe via built-in Windows IExpress..." -ForegroundColor Yellow
    $StageDir = Join-Path $RootDir "build\iexpress_stage"
    if (Test-Path $StageDir) { Remove-Item $StageDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

    $PayloadZip = Join-Path $StageDir "payload.zip"
    Compress-Archive -Path $Vst3Bundle -DestinationPath $PayloadZip -Force

    $InstallCmd = @'
@echo off
setlocal
set "TARGET=%CommonProgramFiles%\VST3"
if not exist "%TARGET%" mkdir "%TARGET%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Path '%~dp0payload.zip' -DestinationPath '%TARGET%' -Force"
if %errorlevel% equ 0 (
    echo Installed Fidiu Compressor.vst3 to "%TARGET%\Fidiu Compressor.vst3"
    msg %username% "Fidiu Compressor VST3 installed successfully into %TARGET%" >nul 2>&1
) else (
    echo Please run FidiuCompressor-Setup.exe as Administrator to write to Program Files\Common Files\VST3.
    pause
)
'@
    Set-Content -Path (Join-Path $StageDir "install.cmd") -Value $InstallCmd -Encoding ASCII

    $SedContent = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=1
HideExtractAnimation=0
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=Install Fidiu Compressor VST3 into C:\Program Files\Common Files\VST3?
DisplayLicense=
FinishMessage=Fidiu Compressor VST3 has been installed into C:\Program Files\Common Files\VST3.
TargetName=$ExeOut
FriendlyName=Fidiu Compressor VST3 Installer
AppLaunched=cmd.exe /c install.cmd
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
SourceFiles=SourceFiles
[Strings]
FILE0="payload.zip"
FILE1="install.cmd"
[SourceFiles]
SourceFiles0=$StageDir
[SourceFiles0]
%FILE0%=
%FILE1%=
"@
    $SedPath = Join-Path $StageDir "FidiuSetup.sed"
    Set-Content -Path $SedPath -Value $SedContent -Encoding ASCII
    & iexpress.exe /N $SedPath
}

Write-Host "Done! Windows installer ready at: $ExeOut" -ForegroundColor Green

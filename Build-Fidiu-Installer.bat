@echo off
echo Building Fidiu Compressor VST3 and FidiuCompressor-Setup.exe...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\build_windows_exe.ps1"
pause

# Fidiu Compressor — VST3 & Windows .EXE Installer Guide

Fidiu Compressor is a precision dynamics processor inspired by FabFilter Pro-C 3, featuring:
- 8 Compression Styles (Clean, Classic, Opto, Vocal, Mastering, Bus, Punch, Pumping)
- Sample-accurate Lookahead delay line (0–20 ms) with DAW PDC (`setLatencySamples`)
- Cascaded 24/48 dB/oct Butterworth Sidechain HPF/LPF + Bell EQ + External Sidechain Bus
- Analog Sound Coloriture (Pure, Tube, Tape, Transformer) + Oversampling

## 1. Build Windows Installer (`FidiuCompressor-Setup.exe`)

Run `Build-Fidiu-Installer.bat` on Windows (requires CMake + Visual Studio Build Tools).
It compiles `Fidiu Compressor.vst3` and automatically packages **`dist-installer/FidiuCompressor-Setup.exe`** using Inno Setup 6, NSIS, or Windows' built-in `iexpress.exe`.

Running `FidiuCompressor-Setup.exe` automatically installs `Fidiu Compressor.vst3` into the standard Windows VST3 directory:
`C:\Program Files\Common Files\VST3\Fidiu Compressor.vst3`

## 2. Cloud Build via GitHub Actions (No Local Visual Studio Required)

Push this folder to GitHub and trigger the included `.github/workflows/build-windows-exe.yml` workflow to download `FidiuCompressor-Setup.exe` directly from GitHub Actions Artifacts.

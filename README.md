# Halden Reverb 6

Modulated FDN/tank stereo reverb built with **JUCE 8** and **CMake** (`1000x600` fixed hardware rack faceplate with brushed dark anodized aluminium, rack ears, 4 corner screws, knurled skeuomorphic knobs with printed tick marks, dual analog **Input/Output VU meters**, scanlined green/amber **LCD** with FFT spectrum + decay/EQ curves, **Reverb Ducking**, and switchable **Post-EQ** bypassed by default at **500 Hz** & **6000 Hz**).

## Formats Built Automatically
- **macOS (Universal arm64 + x86_64)**: `.vst3` and `.component` (Audio Unit)
- **Windows (x64)**: `.vst3`

## Installing Built Plugins
1. Download the archive from the **Actions** tab (or from **Halden Studio → Actions Builds**).
2. Extract the archive:
   - **macOS VST3**: copy `.vst3` to `/Library/Audio/Plug-Ins/VST3/`
   - **macOS AU**: copy `.component` to `/Library/Audio/Plug-Ins/Components/`
   - **Windows VST3**: copy `.vst3` to `C:\Program Files\Common Files\VST3\`
3. On macOS, if Gatekeeper warns about unsigned plugins, run:
   ```bash
   xattr -cr "/Library/Audio/Plug-Ins/VST3/Halden Reverb 6.vst3"
   xattr -cr "/Library/Audio/Plug-Ins/Components/Halden Reverb 6.component"
   ```

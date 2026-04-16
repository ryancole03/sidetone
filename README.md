# Sidetone

*Vibe-coded slop, from me to you.*

A Windows application that captures microphone input and plays it through speakers/headphones with noise suppression, providing real-time sidetone for if you can't stand not being able to hear yourself when talking on discord/games.

## Features

- Real-time microphone passthrough with low-latency WASAPI
- Noise suppression using SpeexDSP
- Adjustable playback volume, noise suppression, and input gain
- Modern flat UI with rounded corners

## Dependencies

| Library | Version | License | Link |
|---------|---------|---------|------|
| libspeexdsp | 1.2.0+ | BSD/Xiph.org | https://github.com/xiph/speexdsp |
| Speex | 1.2.0+ | BSD/Xiph.org | https://github.com/xiph/speex |
| Windows SDK | 10.0+ | Microsoft | (bundled with Windows) |
| .NET | 5.0+ | MIT | https://dotnet.microsoft.com/ |

## Build Requirements

- Visual Studio 2019+ or Build Tools for MSVC
- Windows 10 SDK
- [.NET 5.0 Runtime](https://dotnet.microsoft.com/download/dotnet/5.0) (or SDK)
- [vcpkg](https://github.com/Microsoft/vcpkg) for libspeexdsp

## Build Instructions

### Prerequisites

1. Install vcpkg:
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   .\vcpkg\bootstrap-vcpkg.bat
   ```

2. Install dependencies:
   ```powershell
   vcpkg install speexdsp:x64-windows
   ```

### Building

```powershell
cd app
.\build.bat
```

Output: `app\bin\Release\net5.0-windows\Sidetone.exe`

## Usage

Run `Sidetone.exe` and click "Stop" to begin audio passthrough.

Adjust sliders to control:
- **Playback Volume**: Output volume (0-200%)
- **Noise Suppression**: Noise reduction strength (0-100%)
- **Microphone Gain**: Input gain (0-200%)

## License

Copyright (c) 2026 Ryan Cole

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License** as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [GNU General Public License](LICENSE) for details.

### Third-Party Licenses

- **libspeexdsp**: [Xiph.Org BSD License](https://github.com/xiph/speexdsp/blob/master/COPYING)
- **Speex**: [Xiph.Org BSD License](https://github.com/xiph/speex/blob/master/COPYING)
- **.NET**: [MIT License](https://github.com/dotnet/runtime/blob/main/LICENSE.TXT)

# Changelog

## [1.1.0] - 2026-04-17

### Added
- Noise Gate feature - combines Speex VAD with RMS threshold to suppress warbly artifacts when not talking
- Adjustable noise gate threshold via UI slider (0-100%)
- Buffer size adjustment in UI (64-1024 frames)
- Sample rate display showing input/output Hz

### Changed
- build.bat now auto-detects MSVC, Windows SDK, and vcpkg locations (no hardcoded paths)

### Fixed
- Resolved warbly/distorted artifacts from noise suppression when idle
- Fixed gate chattering at syllable boundaries by adding hold counter and slower attack
- Reduced residual static when gate engages by tuning hold frames and attack coefficient

### Technical Details
- Gate implementation: VAD + RMS hybrid with 40-frame hold (~40ms)
- Attack coefficient: 0.05, Release coefficient: 0.02
- Threshold formula: gatePercent * 0.0004

### Known Limitations
- Bluetooth headphones may have inherent latency/distortion issues beyond app-level fixes
- Sample rate mismatch between input/output devices may cause issues

## [1.0.0] - 2026-02-15

### Added
- Initial release
- Real-time microphone passthrough via WASAPI
- Noise suppression using SpeexDSP
- Adjustable playback volume, noise suppression, and input gain
- Modern flat UI with device selection
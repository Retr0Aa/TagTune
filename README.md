# TagTune

An audio metadata manager UI.

## Features
- Top table for loaded songs
- Bottom tag editor for the selected file
- Cover art preview
- Direct in-file metadata editing with TagLib
- Open files or entire folders for batch loading
- Recent files and folders menu
- Save shortcut for the currently selected track
- Native macOS menu bar integration

## Supported formats
- MP3
- FLAC
- M4A / MP4
- WAV and OGG when the tag backend supports them

## Build
```bash
cmake -S . -B build
cmake --build build
```

## Run
```bash
./build/TagTune
```

## Notes
The editor writes metadata directly into the audio file rather than using JSON sidecar files.

On macOS, the app uses the native global menu bar and keeps file actions there instead of duplicating them in a second top toolbar.

Recent paths are stored with `QSettings`, so they persist between launches.



# TagTune

## Packaging for Production

Use the `.icns` file in `resources/icons` when building the macOS app bundle. The PNG stays the runtime icon for Windows and Linux.

### 1. Install PyInstaller
```bash
python3 -m pip install pyinstaller
```

### 2. Build the `.app` and `.dmg`
Run the helper script from the project root on macOS:
```bash
./scripts/build_macos.sh
```

If your shell says permission denied, make it executable once:
```bash
chmod +x scripts/build_macos.sh
```

The script:
- finds the `.icns` file in `resources/icons`
- clears old `build/` and `dist/` output
- builds `dist/TagTune.app`
- packages `dist/TagTune.dmg`

### 3. Manual fallback
If you want to do it by hand, the script runs the equivalent of:
```bash
pyinstaller \
  --clean \
  --noconfirm \
  --windowed \
  --name TagTune \
  --icon "$(find resources/icons -maxdepth 1 -name '*.icns' | head -n 1)" \
  --add-data "resources/icons:resources/icons" \
  --collect-all PySide6 \
  --hidden-import mutagen \
  main.py
```

Then create the DMG:
```bash
hdiutil create \
  -volname TagTune \
  -srcfolder dist/TagTune.app \
  -ov \
  -format UDZO \
  dist/TagTune.dmg
```

### 4. Verify the result
- Open `dist/TagTune.app` and confirm the icon appears in Finder and the Dock.
- Mount `dist/TagTune.dmg` and confirm the app launches correctly.

## Notes
- The app uses the `.icns` file on macOS and the PNG fallback on other platforms.
- For Windows builds, use the same PyInstaller flow, but keep the PNG icon and adjust `--add-data` path separators if needed.

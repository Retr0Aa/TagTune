#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT_DIR"

APP_NAME="TagTune"
ICON_PATH="$(find "$ROOT_DIR/resources/icons" -maxdepth 1 -name '*.icns' | head -n 1)"
VENDOR_DIR="$ROOT_DIR/vendor"
ICONS_DIR="$ROOT_DIR/resources/icons"

if [ -z "${ICON_PATH:-}" ] || [ ! -f "$ICON_PATH" ]; then
    echo "Could not find a macOS .icns file in resources/icons" >&2
    exit 1
fi

PYTHON_BIN="${PYTHON_BIN:-python3}"
if [ -x ".venv/bin/python" ]; then
    PYTHON_BIN=".venv/bin/python"
fi

export PYINSTALLER_CONFIG_DIR="$ROOT_DIR/.pyinstaller"

rm -rf build dist "$PYINSTALLER_CONFIG_DIR"
mkdir -p "$PYINSTALLER_CONFIG_DIR/specs"

"$PYTHON_BIN" -m PyInstaller \
    --clean \
    --noconfirm \
    --windowed \
    --name "$APP_NAME" \
    --icon "$ICON_PATH" \
    --paths "$VENDOR_DIR" \
    --specpath "$PYINSTALLER_CONFIG_DIR/specs" \
    --add-data "$ICONS_DIR:resources/icons" \
    --collect-all PySide6 \
    --hidden-import mutagen \
    main.py

APP_BUNDLE="dist/${APP_NAME}.app"
DMG_PATH="dist/${APP_NAME}.dmg"

if [ ! -d "$APP_BUNDLE" ]; then
    echo "PyInstaller did not produce $APP_BUNDLE" >&2
    exit 1
fi

rm -f "$DMG_PATH"
hdiutil create \
    -volname "$APP_NAME" \
    -srcfolder "$APP_BUNDLE" \
    -ov \
    -format UDZO \
    "$DMG_PATH"

echo "Built: $APP_BUNDLE"
echo "Built: $DMG_PATH"
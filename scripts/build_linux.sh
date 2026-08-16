#!/bin/bash
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

APP_NAME="TagTune"
APP_VERSION="${APP_VERSION:-1.0.0}"
ICON_PATH="$ROOT_DIR/resources/icons/Icon-Default.png"
ICONS_DIR="$ROOT_DIR/resources/icons"
APPIMAGE_ARCH="${APPIMAGE_ARCH:-$(uname -m)}"

normalize_arch() {
    case "$1" in
        amd64|x86_64)
            printf '%s' "x86_64"
            ;;
        arm64|aarch64)
            printf '%s' "aarch64"
            ;;
        *)
            printf '%s' "$1"
            ;;
    esac
}

APPIMAGE_ARCH="$(normalize_arch "$APPIMAGE_ARCH")"

if [ ! -f "$ICON_PATH" ]; then
    echo "Could not find PNG icon at $ICON_PATH" >&2
    exit 1
fi

PYTHON_BIN="${PYTHON_BIN:-python3}"
if [ -x ".venv/bin/python" ]; then
    PYTHON_BIN=".venv/bin/python"
fi

export PYINSTALLER_CONFIG_DIR="$ROOT_DIR/.pyinstaller"

echo "Building $APP_NAME for Linux .AppImage..."

# Clean up old builds
rm -rf build dist "$PYINSTALLER_CONFIG_DIR"
mkdir -p "$PYINSTALLER_CONFIG_DIR/specs"

# Build with PyInstaller
echo "Running PyInstaller..."
"$PYTHON_BIN" -m PyInstaller \
    --clean \
    --noconfirm \
    --windowed \
    --name "$APP_NAME" \
    --icon "$ICON_PATH" \
    --specpath "$PYINSTALLER_CONFIG_DIR/specs" \
    --add-data "$ICONS_DIR:resources/icons" \
    --collect-all PySide6 \
    --hidden-import mutagen \
    main.py

# Create AppDir structure for AppImage
echo "Creating AppImage structure..."
APPDIR="dist/${APP_NAME}.AppDir"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/pixmaps"

# Copy the built application
cp -r "dist/$APP_NAME"/* "$APPDIR/usr/bin/" || true

# Create desktop entry in both locations (AppDir root and usr/share/applications)
cat > "$APPDIR/TagTune.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=TagTune
Comment=Music metadata editor
Icon=tagtune
Exec=TagTune %F
Categories=Audio;Utility;
EOF

cat > "$APPDIR/usr/share/applications/${APP_NAME}.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=TagTune
Comment=Music metadata editor
Icon=tagtune
Exec=TagTune %F
Categories=Audio;Utility;
EOF

# Copy icon to standard locations
cp "$ICON_PATH" "$APPDIR/usr/share/icons/hicolor/256x256/apps/tagtune.png"
cp "$ICON_PATH" "$APPDIR/usr/share/pixmaps/tagtune.png"

# Create symlink for icon in AppDir root
ln -sf usr/share/icons/hicolor/256x256/apps/tagtune.png "$APPDIR/tagtune.png" || true

# Create AppRun wrapper script
cat > "$APPDIR/AppRun" << 'APPRUN'
#!/bin/bash
SELF="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$SELF/usr/lib:$LD_LIBRARY_PATH"
exec "$SELF/usr/bin/TagTune" "$@"
APPRUN

chmod +x "$APPDIR/AppRun"

# Check for AppImage tools
APPIMAGETOOL=""
if command -v appimagetool &> /dev/null; then
    APPIMAGETOOL="appimagetool"
elif [ -f "/tmp/appimagetool.AppImage" ]; then
    APPIMAGETOOL="/tmp/appimagetool.AppImage"
fi

if [ -z "$APPIMAGETOOL" ]; then
    echo "Downloading appimagetool..."
    wget -q "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${APPIMAGE_ARCH}.AppImage" -O "/tmp/appimagetool.AppImage"
    chmod +x "/tmp/appimagetool.AppImage"
    APPIMAGETOOL="/tmp/appimagetool.AppImage"
fi

# Build AppImage
echo "Building AppImage..."
"$APPIMAGETOOL" "$APPDIR" "dist/${APP_NAME}-${APP_VERSION}-${APPIMAGE_ARCH}.AppImage"

# Make it executable
chmod +x "dist/${APP_NAME}-${APP_VERSION}-${APPIMAGE_ARCH}.AppImage"

echo ""
echo "✓ Success! AppImage created at: dist/${APP_NAME}-${APP_VERSION}-${APPIMAGE_ARCH}.AppImage"
echo ""
echo "To run the app:"
echo "  ./dist/${APP_NAME}-${APP_VERSION}-${APPIMAGE_ARCH}.AppImage"
echo ""

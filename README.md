# TagTune

## Building and Running

To run TagTune, follow the commands below to create an environment, install dependencies, and start the app.

### 1. Open a terminal in the project root
```bash
cd /path/to/TagTune
```

### 2. Create a virtual environment
```bash
python3 -m venv .venv
```

### 3. Activate the virtual environment
```bash
source .venv/bin/activate
```

On Windows:
```bash
.venv\Scripts\activate
```

### 4. Upgrade pip
```bash
python -m pip install --upgrade pip
```

### 5. Install the project dependencies
```bash
python -m pip install -r requirements.txt
```

### 6. Run TagTune
```bash
python main.py
```

## Packaging for Production

### macOS .dmg

Use the `.icns` file in `resources/icons` when building the macOS app bundle. The PNG stays the runtime icon for Windows and Linux.

#### 1. Install PyInstaller
```bash
python3 -m pip install pyinstaller
```

#### 2. Build the `.app` and `.dmg`
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

#### 4. Verify the result
- Open `dist/TagTune.app` and confirm the icon appears in Finder and the Dock.
- Mount `dist/TagTune.dmg` and confirm the app launches correctly.

#### Notes
- The app uses the `.icns` file on macOS and the PNG fallback on other platforms.

### Linux .AppImage

Build a self-contained Linux AppImage that works across different distributions.

#### 1. Install dependencies

**For Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install python3-dev libpython3-dev libfuse2
```

**For Fedora/RHEL:**
```bash
sudo dnf install python3-devel fuse-libs
```

AppImages require FUSE to run. The above commands install the necessary libraries.

#### 2. Install PyInstaller
```bash
python3 -m pip install pyinstaller
```

#### 3. Install AppImage tools (optional)

The build script can automatically download and use `appimagetool` if it's not found. However, you can optionally install it system-wide:

```bash
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage -O /tmp/appimagetool.AppImage
chmod +x /tmp/appimagetool.AppImage
sudo /tmp/appimagetool.AppImage --install
```

Or manually (without sudo):
```bash
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage -O /tmp/appimagetool.AppImage
chmod +x /tmp/appimagetool.AppImage
```

#### 4. Build the .AppImage

Run the build script from the project root on Linux:
```bash
./scripts/build_linux.sh
```

If your shell says permission denied, make it executable once:
```bash
chmod +x scripts/build_linux.sh
```

The script:
- uses the `.png` file in `resources/icons` as the application icon
- clears old `build/` and `dist/` output
- builds the application with PyInstaller
- creates an AppDir structure with proper desktop entry and icon paths
- packages `dist/TagTune-1.0.0-x86_64.AppImage`

#### 5. Run the AppImage

```bash
./dist/TagTune-1.0.0-x86_64.AppImage
```

You can also:
- Make it available in your PATH or Applications menu
- Double-click it from your file manager to run it
- Integrate it with your system's app launcher

#### 6. Verify the result
- Run the AppImage and confirm the application launches
- Check that the TagTune icon appears in your desktop environment
- Test music metadata editing functionality

#### Notes
- The AppImage is a self-contained executable that includes all dependencies
- It works on most Linux distributions (glibc 2.29+)
- The PNG icon is displayed in the application menu and window title
- The AppImage can be distributed as a single file without requiring installation

# TagTune

TagTune is audio metadata editor for audio files. Using it, you can easily manpulate, delete and edit tags such as, *Title*, *Cover art* and much more

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

### GitHub Actions
> [!IMPORTANT]
> This is the preferred way for building your own TagTune artifacts

The repository includes a release workflow at [`.github/workflows/build-installers.yml`](.github/workflows/build-installers.yml). It can build each platform on its matching runner and publish the resulting files as artifacts and a GitHub Release.

#### How to run it
You can either push a tag that starts with `v`, or run the workflow manually from the Actions tab.

Tag example:
```bash
git tag v1.0.0
git push origin v1.0.0
```

#### Platform builds
- Windows x64 runs on `windows-latest` and produces `TagTune-<version>-Setup.exe`.
- Ubuntu x64 runs on `ubuntu-latest` and produces `TagTune-<version>-x86_64.AppImage`.
- Ubuntu arm64 runs on `ubuntu-24.04-arm` and produces `TagTune-<version>-aarch64.AppImage`.
- macOS arm64 runs on `macos-14` and produces `TagTune-<version>.dmg` plus `TagTune.app`.

#### What the workflow does
- installs the project dependencies for each job
- runs the existing platform build script for that operating system
- uploads the build outputs as workflow artifacts
- creates a GitHub Release for tag builds and attaches the same files

### Windows .exe installer

Build a Windows installer executable for TagTune.

#### 1. Install packaging dependencies
```powershell
python -m pip install pyinstaller
```

Install Inno Setup 6 if you want the Windows installer executable:
- https://jrsoftware.org/isinfo.php

#### 2. Build the app and installer
Run the helper script from the project root on Windows:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1
```

The script:
- clears old `build/` and `dist/` output
- builds `dist/TagTune` with PyInstaller
- packages `dist/TagTune-1.0.0-Setup.exe` with Inno Setup when `ISCC.exe` is available

#### 3. Verify the result
- Run `dist/TagTune-1.0.0-Setup.exe` and complete installation.
- Launch TagTune from the Start Menu or desktop shortcut.

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

> [!NOTE]
> The app uses the `.icns` file on macOS and the PNG fallback on other platforms.

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

The build script can automatically download and use `appimagetool` if it's not found. It selects the correct architecture-specific AppImage for the current runner. However, you can optionally install it system-wide:

```bash
wget https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage -O /tmp/appimagetool.AppImage
chmod +x /tmp/appimagetool.AppImage
sudo /tmp/appimagetool.AppImage --install
```

Or manually (without sudo):
```bash
wget https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage -O /tmp/appimagetool.AppImage
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
- packages `dist/TagTune-1.0.0-x86_64.AppImage` on x64 or `dist/TagTune-1.0.0-aarch64.AppImage` on arm64

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

> [!NOTE]
> - The AppImage is a self-contained executable that includes all dependencies
> - It works on most Linux distributions (glibc 2.29+)
> - The PNG icon is displayed in the application menu and window title
> - The AppImage can be distributed as a single file without requiring installation

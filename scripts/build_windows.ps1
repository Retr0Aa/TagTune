$ErrorActionPreference = "Stop"

$ROOT_DIR = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $ROOT_DIR

$APP_NAME = "TagTune"
$APP_VERSION = "1.0.0"
$ICONS_DIR = Join-Path $ROOT_DIR "resources/icons"
$WINDOWS_ICON = Get-ChildItem -Path $ICONS_DIR -Filter "*.ico" -File -ErrorAction SilentlyContinue | Select-Object -First 1
$PNG_ICON = Get-ChildItem -Path $ICONS_DIR -Filter "*.png" -File -ErrorAction SilentlyContinue | Select-Object -First 1
$VENDOR_DIR = Join-Path $ROOT_DIR "vendor"
$PYINSTALLER_CONFIG_DIR = Join-Path $ROOT_DIR ".pyinstaller"
$GENERATED_WINDOWS_ICON = Join-Path $ICONS_DIR "app-icon.ico"

$venvPython = Join-Path $ROOT_DIR ".venv\Scripts\python.exe"
if (Test-Path $venvPython) {
    $pythonCommand = $venvPython
    $pythonPrefixArgs = @()
} elseif (Get-Command py.exe -ErrorAction SilentlyContinue) {
    $pythonCommand = "py.exe"
    $pythonPrefixArgs = @("-3")
} elseif (Get-Command python.exe -ErrorAction SilentlyContinue) {
    $pythonCommand = "python.exe"
    $pythonPrefixArgs = @()
} else {
    throw "Could not find Python. Install Python 3 or create a .venv first."
}

Write-Host "Building $APP_NAME for Windows installer..."

if (Test-Path "build") { Remove-Item -Recurse -Force "build" }
if (Test-Path "dist") { Remove-Item -Recurse -Force "dist" }
if (Test-Path $PYINSTALLER_CONFIG_DIR) { Remove-Item -Recurse -Force $PYINSTALLER_CONFIG_DIR }

New-Item -ItemType Directory -Path $PYINSTALLER_CONFIG_DIR -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $PYINSTALLER_CONFIG_DIR "specs") -Force | Out-Null

if (-not $WINDOWS_ICON -and $PNG_ICON) {
    Write-Host "Converting $($PNG_ICON.Name) to a Windows .ico icon..."
    $iconScriptPath = Join-Path $PYINSTALLER_CONFIG_DIR "make_icon.py"
    $iconScript = @'
import importlib.util
import subprocess
import sys
from pathlib import Path

if importlib.util.find_spec("PIL") is None:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "pillow"])

from PIL import Image

src = Path(sys.argv[1])
dst = Path(sys.argv[2])

with Image.open(src) as img:
    img = img.convert("RGBA")
    img.save(dst, format="ICO", sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
'@
    Set-Content -Path $iconScriptPath -Value $iconScript -Encoding UTF8
    & $pythonCommand @pythonPrefixArgs $iconScriptPath $PNG_ICON.FullName $GENERATED_WINDOWS_ICON

    if (Test-Path $GENERATED_WINDOWS_ICON) {
        $WINDOWS_ICON = Get-Item $GENERATED_WINDOWS_ICON
    }
}

$pyInstallerArgs = @(
    "-m", "PyInstaller",
    "--clean",
    "--noconfirm",
    "--windowed",
    "--name", $APP_NAME,
    "--specpath", (Join-Path $PYINSTALLER_CONFIG_DIR "specs"),
    "--add-data", "$ICONS_DIR;resources/icons",
    "--collect-all", "PySide6",
    "--hidden-import", "mutagen",
    "main.py"
)

if ($WINDOWS_ICON) {
    $pyInstallerArgs = @("-m", "PyInstaller", "--icon", $WINDOWS_ICON.FullName) + $pyInstallerArgs[2..($pyInstallerArgs.Count - 1)]
} else {
    Write-Host "No .ico icon found in resources/icons. Using default executable icon."
}

if (Test-Path $VENDOR_DIR) {
    $pyInstallerArgs = @("-m", "PyInstaller", "--paths", $VENDOR_DIR) + $pyInstallerArgs[2..($pyInstallerArgs.Count - 1)]
}

& $pythonCommand @pythonPrefixArgs @pyInstallerArgs

$distDir = Join-Path $ROOT_DIR "dist"
$appDistDir = Join-Path $distDir $APP_NAME
if (-not (Test-Path $appDistDir)) {
    throw "PyInstaller did not produce $appDistDir"
}

$innoCompiler = $null
$isccCommand = Get-Command iscc.exe -ErrorAction SilentlyContinue
if ($isccCommand) {
    $innoCompiler = $isccCommand.Source
} else {
    $candidatePaths = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    )
    foreach ($candidate in $candidatePaths) {
        if (Test-Path $candidate) {
            $innoCompiler = $candidate
            break
        }
    }
}

if (-not $innoCompiler) {
    Write-Warning "Could not find Inno Setup compiler (ISCC.exe). Install Inno Setup 6 from https://jrsoftware.org/isinfo.php to build the installer. Skipping installer generation."
    Write-Host "Success! Built: $appDistDir"
    Write-Host "Installer not built because ISCC.exe was not found."
    Write-Host ""
    exit 0
}

$installerBaseName = "$APP_NAME-$APP_VERSION-Setup"
$issPath = Join-Path $distDir "$APP_NAME-installer.iss"

$innoScript = @"
[Setup]
AppId={{B88F5569-A557-4E6E-B24A-1F0B6E71A8D2}}
AppName=$APP_NAME
AppVersion=$APP_VERSION
DefaultDirName={autopf}\$APP_NAME
DefaultGroupName=$APP_NAME
OutputDir="$distDir"
OutputBaseFilename=$installerBaseName
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "$appDistDir\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\$APP_NAME"; Filename: "{app}\$APP_NAME.exe"
Name: "{autodesktop}\$APP_NAME"; Filename: "{app}\$APP_NAME.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\$APP_NAME.exe"; Description: "Launch $APP_NAME"; Flags: nowait postinstall skipifsilent
"@

Set-Content -Path $issPath -Value $innoScript -Encoding ASCII

& $innoCompiler "/Qp" $issPath

$installerPath = Join-Path $distDir "$installerBaseName.exe"
if (-not (Test-Path $installerPath)) {
    throw "Inno Setup did not produce $installerPath"
}

Write-Host ""
Write-Host "Success! Built: $appDistDir"
Write-Host "Success! Built: $installerPath"
Write-Host ""
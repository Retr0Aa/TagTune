from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtGui import QIcon


def app_icon() -> QIcon:
    if getattr(sys, 'frozen', False) and hasattr(sys, '_MEIPASS'):
        root = Path(sys._MEIPASS)
    else:
        root = Path(__file__).resolve().parents[2]

    icons_dir = root / 'resources' / 'icons'

    ico_files = sorted(icons_dir.glob('*.ico'))
    if ico_files:
        return QIcon(str(ico_files[0]))

    if sys.platform == 'darwin':
        icns_files = sorted(icons_dir.glob('*.icns'))
        if icns_files:
            return QIcon(str(icns_files[0]))

    png_files = sorted(icons_dir.glob('*.png'))
    if png_files:
        return QIcon(str(png_files[0]))

    icns_files = sorted(icons_dir.glob('*.icns'))
    if icns_files:
        return QIcon(str(icns_files[0]))

    return QIcon()

from dataclasses import dataclass
from PySide6.QtGui import QPixmap

@dataclass
class Track:
    filePath: str = ""
    title: str = ""
    artist: str = ""
    album: str = ""
    albumArtist: str = ""
    genre: str = ""
    comment: str = ""
    composer: str = ""
    year: str = ""
    trackNumber: str = ""
    discNumber: str = ""
    bpm: str = ""
    coverArt: QPixmap | None = None
    
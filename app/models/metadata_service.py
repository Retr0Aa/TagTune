from __future__ import annotations

import base64
import sys
from pathlib import Path
from typing import Iterable

from PySide6.QtCore import QDate, QFileInfo
from PySide6.QtGui import QPixmap

from app.models.track import Track

_VENDOR_DIR = Path(__file__).resolve().parents[2] / "vendor"
if _VENDOR_DIR.exists():
    vendor_path = str(_VENDOR_DIR)
    if vendor_path not in sys.path:
        sys.path.insert(0, vendor_path)

from mutagen import File as MutagenFile
from mutagen.flac import FLAC, Picture
from mutagen.id3 import APIC, COMM, ID3, ID3NoHeaderError, TALB, TCOM, TCON, TDRC, TIT2, TPE1, TPE2, TPOS, TRCK, TBPM
from mutagen.mp3 import MP3
from mutagen.mp4 import MP4, MP4Cover
from mutagen.oggvorbis import OggVorbis
from mutagen.wave import WAVE


class MetadataService:
    _SUPPORTED_EXTENSIONS = {
        ".aac",
        ".flac",
        ".m4a",
        ".mp3",
        ".mp4",
        ".ogg",
        ".wav",
        ".wave",
    }

    def load_tracks(self, file_paths: Iterable[str]) -> list[Track]:
        return [self.load_track(path) for path in file_paths]

    def load_track(self, file_path: str) -> Track:
        track = self._blank_track(file_path)
        self._read_metadata(track)
        return self._apply_defaults(track)

    def save_track(self, track: Track, error_message: str | None = None) -> bool:
        del error_message

        try:
            return self._write_metadata(track)
        except Exception:
            return False

    def is_audio_file(self, file_path: str) -> bool:
        return Path(file_path).suffix.lower() in self._SUPPORTED_EXTENSIONS

    def scan_folder(self, folder_path: str) -> list[str]:
        root = Path(folder_path)
        if not root.exists():
            return []

        files: list[str] = []
        for candidate in root.rglob("*"):
            if candidate.is_file() and self.is_audio_file(str(candidate)):
                files.append(str(candidate))
        return files

    def _blank_track(self, file_path: str) -> Track:
        info = QFileInfo(file_path)
        base_name = info.baseName().strip() or "Unknown Track"
        return Track(
            filePath=file_path,
            title=base_name,
            artist="Unknown Artist",
            album="Unknown Album",
            albumArtist="Unknown Artist",
            genre="Unknown",
            year=str(QDate.currentDate().year()),
        )

    def _apply_defaults(self, track: Track) -> Track:
        info = QFileInfo(track.filePath)
        if not track.title.strip():
            track.title = info.baseName() or "Unknown Track"
        if not track.artist.strip():
            track.artist = "Unknown Artist"
        if not track.album.strip():
            track.album = "Unknown Album"
        if not track.albumArtist.strip():
            track.albumArtist = "Unknown Artist"
        if not track.genre.strip():
            track.genre = "Unknown"
        if not track.year.strip():
            track.year = str(QDate.currentDate().year())
        return track

    def _open_audio(self, file_path: str):
        suffix = Path(file_path).suffix.lower()
        try:
            if suffix == ".mp3":
                return MutagenFile(file_path, easy=False)
            if suffix in {".wav", ".wave"}:
                return WAVE(file_path)
            if suffix == ".flac":
                return FLAC(file_path)
            if suffix in {".m4a", ".mp4", ".aac"}:
                return MP4(file_path)
            if suffix == ".ogg":
                return OggVorbis(file_path)
            return MutagenFile(file_path, easy=False)
        except Exception:
            return None

    def _read_metadata(self, track: Track) -> None:
        audio = self._open_audio(track.filePath)
        if audio is None:
            return

        if isinstance(audio, (MP3, WAVE)):
            self._read_id3(track, getattr(audio, "tags", None))
            return

        if isinstance(audio, FLAC):
            self._read_flac(track, audio)
            return

        if isinstance(audio, OggVorbis):
            self._read_vorbis(track, audio)
            return

        if isinstance(audio, MP4):
            self._read_mp4(track, audio)
            return

        if hasattr(audio, "tags") and isinstance(audio.tags, ID3):
            self._read_id3(track, audio.tags)

    def _write_metadata(self, track: Track) -> bool:
        audio = self._open_audio_for_write(track.filePath)
        if audio is None:
            return False

        suffix = Path(track.filePath).suffix.lower()
        if suffix == ".mp3" or isinstance(audio, (MP3, WAVE)):
            return self._write_id3(track, audio)
        if suffix == ".flac" and isinstance(audio, FLAC):
            return self._write_flac(track, audio)
        if suffix == ".ogg" and isinstance(audio, OggVorbis):
            return self._write_vorbis(track, audio)
        if suffix in {".m4a", ".mp4", ".aac"} and isinstance(audio, MP4):
            return self._write_mp4(track, audio)
        if hasattr(audio, "save"):
            return self._write_generic(track, audio)
        return False

    def _open_audio_for_write(self, file_path: str):
        suffix = Path(file_path).suffix.lower()
        try:
            if suffix == ".mp3":
                return MutagenFile(file_path, easy=False) or MutagenFile(file_path)
            if suffix in {".wav", ".wave"}:
                return WAVE(file_path)
            if suffix == ".flac":
                return FLAC(file_path)
            if suffix in {".m4a", ".mp4", ".aac"}:
                return MP4(file_path)
            if suffix == ".ogg":
                return OggVorbis(file_path)
            return MutagenFile(file_path, easy=False)
        except Exception:
            return None

    def _read_id3(self, track: Track, tags: ID3 | None) -> None:
        if tags is None:
            return

        track.title = self._id3_text(tags, "TIT2") or track.title
        track.artist = self._id3_text(tags, "TPE1") or track.artist
        track.album = self._id3_text(tags, "TALB") or track.album
        track.albumArtist = self._id3_text(tags, "TPE2") or track.albumArtist
        track.genre = self._id3_text(tags, "TCON") or track.genre
        track.composer = self._id3_text(tags, "TCOM") or track.composer
        track.year = self._id3_text(tags, "TDRC", "TYER") or track.year
        track.trackNumber = self._id3_text(tags, "TRCK") or track.trackNumber
        track.discNumber = self._id3_text(tags, "TPOS") or track.discNumber
        track.bpm = self._id3_text(tags, "TBPM") or track.bpm
        comment = self._id3_comment(tags)
        if comment:
            track.comment = comment

        cover = self._id3_cover(tags)
        if cover is not None:
            track.coverArt = cover

    def _read_flac(self, track: Track, audio: FLAC) -> None:
        tags = audio.tags or {}
        track.title = self._tag_text(tags, "TITLE") or self._tag_text(tags, "title") or track.title
        track.artist = self._tag_text(tags, "ARTIST") or self._tag_text(tags, "artist") or track.artist
        track.album = self._tag_text(tags, "ALBUM") or self._tag_text(tags, "album") or track.album
        track.albumArtist = (
            self._tag_text(tags, "ALBUMARTIST")
            or self._tag_text(tags, "albumartist")
            or self._tag_text(tags, "ALBUM ARTIST")
            or track.albumArtist
        )
        track.genre = self._tag_text(tags, "GENRE") or self._tag_text(tags, "genre") or track.genre
        track.comment = self._tag_text(tags, "COMMENT") or self._tag_text(tags, "comment") or track.comment
        track.composer = self._tag_text(tags, "COMPOSER") or self._tag_text(tags, "composer") or track.composer
        track.year = (
            self._tag_text(tags, "DATE")
            or self._tag_text(tags, "YEAR")
            or self._tag_text(tags, "date")
            or self._tag_text(tags, "year")
            or track.year
        )
        track.trackNumber = (
            self._tag_text(tags, "TRACKNUMBER")
            or self._tag_text(tags, "tracknumber")
            or track.trackNumber
        )
        track.discNumber = (
            self._tag_text(tags, "DISCNUMBER")
            or self._tag_text(tags, "discnumber")
            or track.discNumber
        )
        track.bpm = self._tag_text(tags, "BPM") or self._tag_text(tags, "bpm") or track.bpm

        if audio.pictures:
            track.coverArt = self._pixmap_from_picture(audio.pictures[0])

    def _read_vorbis(self, track: Track, audio: OggVorbis) -> None:
        tags = audio.tags or {}
        track.title = self._tag_text(tags, "TITLE", "title") or track.title
        track.artist = self._tag_text(tags, "ARTIST", "artist") or track.artist
        track.album = self._tag_text(tags, "ALBUM", "album") or track.album
        track.albumArtist = (
            self._tag_text(tags, "ALBUMARTIST", "albumartist", "ALBUM ARTIST") or track.albumArtist
        )
        track.genre = self._tag_text(tags, "GENRE", "genre") or track.genre
        track.comment = self._tag_text(tags, "COMMENT", "comment") or track.comment
        track.composer = self._tag_text(tags, "COMPOSER", "composer") or track.composer
        track.year = self._tag_text(tags, "DATE", "date", "YEAR", "year") or track.year
        track.trackNumber = self._tag_text(tags, "TRACKNUMBER", "tracknumber") or track.trackNumber
        track.discNumber = self._tag_text(tags, "DISCNUMBER", "discnumber") or track.discNumber
        track.bpm = self._tag_text(tags, "BPM", "bpm") or track.bpm

        picture_data = self._first_tag_value(tags, "METADATA_BLOCK_PICTURE")
        if picture_data:
            try:
                picture = Picture()
                picture.load(base64.b64decode(picture_data))
                track.coverArt = self._pixmap_from_bytes(picture.data)
            except Exception:
                pass

    def _read_mp4(self, track: Track, audio: MP4) -> None:
        tags = audio.tags or {}
        track.title = self._mp4_text(tags, "©nam") or track.title
        track.artist = self._mp4_text(tags, "©ART") or track.artist
        track.album = self._mp4_text(tags, "©alb") or track.album
        track.albumArtist = self._mp4_text(tags, "aART") or track.albumArtist
        track.genre = self._mp4_text(tags, "©gen") or track.genre
        track.comment = self._mp4_text(tags, "©cmt") or track.comment
        track.composer = self._mp4_text(tags, "©wrt") or track.composer
        track.year = self._mp4_text(tags, "©day") or track.year
        track.trackNumber = self._mp4_pair(tags, "trkn") or track.trackNumber
        track.discNumber = self._mp4_pair(tags, "disk") or track.discNumber
        track.bpm = self._mp4_int(tags, "tmpo") or track.bpm

        cover = tags.get("covr")
        if cover:
            try:
                track.coverArt = self._pixmap_from_bytes(bytes(cover[0]))
            except Exception:
                pass

    def _write_id3(self, track: Track, audio) -> bool:
        tags = audio.tags
        if tags is None:
            try:
                audio.add_tags()
                tags = audio.tags
            except ID3NoHeaderError:
                audio.tags = ID3()
                tags = audio.tags

        if tags is None:
            return False

        self._set_id3_text(tags, "TIT2", track.title)
        self._set_id3_text(tags, "TPE1", track.artist)
        self._set_id3_text(tags, "TALB", track.album)
        self._set_id3_text(tags, "TPE2", track.albumArtist)
        self._set_id3_text(tags, "TCON", track.genre)
        self._set_id3_text(tags, "TCOM", track.composer)
        self._set_id3_text(tags, "TDRC", track.year)
        self._set_id3_text(tags, "TRCK", track.trackNumber)
        self._set_id3_text(tags, "TPOS", track.discNumber)
        self._set_id3_text(tags, "TBPM", track.bpm)
        self._set_id3_comment(tags, track.comment)
        self._set_id3_cover(tags, track.coverArt)

        audio.save(v2_version=3)
        return True

    def _write_flac(self, track: Track, audio: FLAC) -> bool:
        self._ensure_tags(audio)
        tags = audio.tags
        if tags is None:
            return False

        self._set_tag(tags, "TITLE", track.title)
        self._set_tag(tags, "ARTIST", track.artist)
        self._set_tag(tags, "ALBUM", track.album)
        self._set_tag(tags, "ALBUMARTIST", track.albumArtist)
        self._set_tag(tags, "GENRE", track.genre)
        self._set_tag(tags, "COMMENT", track.comment)
        self._set_tag(tags, "COMPOSER", track.composer)
        self._set_tag(tags, "DATE", track.year)
        self._set_tag(tags, "TRACKNUMBER", track.trackNumber)
        self._set_tag(tags, "DISCNUMBER", track.discNumber)
        self._set_tag(tags, "BPM", track.bpm)

        audio.clear_pictures()
        self._set_flac_cover(audio, track.coverArt)
        audio.save()
        return True

    def _write_vorbis(self, track: Track, audio: OggVorbis) -> bool:
        self._ensure_tags(audio)
        tags = audio.tags
        if tags is None:
            return False

        self._set_tag(tags, "TITLE", track.title)
        self._set_tag(tags, "ARTIST", track.artist)
        self._set_tag(tags, "ALBUM", track.album)
        self._set_tag(tags, "ALBUMARTIST", track.albumArtist)
        self._set_tag(tags, "GENRE", track.genre)
        self._set_tag(tags, "COMMENT", track.comment)
        self._set_tag(tags, "COMPOSER", track.composer)
        self._set_tag(tags, "DATE", track.year)
        self._set_tag(tags, "TRACKNUMBER", track.trackNumber)
        self._set_tag(tags, "DISCNUMBER", track.discNumber)
        self._set_tag(tags, "BPM", track.bpm)

        self._set_vorbis_cover(tags, track.coverArt)
        audio.save()
        return True

    def _write_mp4(self, track: Track, audio: MP4) -> bool:
        self._ensure_tags(audio)
        tags = audio.tags
        if tags is None:
            return False

        self._set_mp4_text(tags, "©nam", track.title)
        self._set_mp4_text(tags, "©ART", track.artist)
        self._set_mp4_text(tags, "©alb", track.album)
        self._set_mp4_text(tags, "aART", track.albumArtist)
        self._set_mp4_text(tags, "©gen", track.genre)
        self._set_mp4_text(tags, "©cmt", track.comment)
        self._set_mp4_text(tags, "©wrt", track.composer)
        self._set_mp4_text(tags, "©day", track.year)
        self._set_mp4_track_pair(tags, "trkn", track.trackNumber)
        self._set_mp4_track_pair(tags, "disk", track.discNumber)
        self._set_mp4_int(tags, "tmpo", track.bpm)
        self._set_mp4_cover(tags, track.coverArt)

        audio.save()
        return True

    def _write_generic(self, track: Track, audio) -> bool:
        if isinstance(audio, ID3):  # pragma: no cover
            return self._write_id3(track, audio)
        return False

    def _ensure_tags(self, audio) -> None:
        if getattr(audio, "tags", None) is None:
            try:
                audio.add_tags()
            except Exception:
                pass

    def _set_tag(self, tags, key: str, value: str) -> None:
        if value.strip():
            tags[key] = [value.strip()]
        elif key in tags:
            del tags[key]

    def _set_mp4_text(self, tags, key: str, value: str) -> None:
        if value.strip():
            tags[key] = [value.strip()]
        elif key in tags:
            del tags[key]

    def _set_mp4_track_pair(self, tags, key: str, value: str) -> None:
        number = self._int_or_none(value)
        if number is None:
            if key in tags:
                del tags[key]
            return
        tags[key] = [(number, 0)]

    def _set_mp4_int(self, tags, key: str, value: str) -> None:
        number = self._int_or_none(value)
        if number is None:
            if key in tags:
                del tags[key]
            return
        tags[key] = [number]

    def _set_id3_text(self, tags: ID3, frame_id: str, value: str) -> None:
        value = value.strip()
        if not value:
            for frame in list(tags.getall(frame_id)):
                tags.delall(frame_id)
            return

        frame_map = {
            "TIT2": TIT2,
            "TPE1": TPE1,
            "TALB": TALB,
            "TPE2": TPE2,
            "TCON": TCON,
            "TCOM": TCOM,
            "TDRC": TDRC,
            "TRCK": TRCK,
            "TPOS": TPOS,
            "TBPM": TBPM,
        }
        frame_cls = frame_map.get(frame_id)
        if frame_cls is None:
            return

        tags.delall(frame_id)
        tags.add(frame_cls(encoding=3, text=[value]))

    def _set_id3_comment(self, tags: ID3, value: str) -> None:
        tags.delall("COMM")
        value = value.strip()
        if not value:
            return
        tags.add(COMM(encoding=3, lang="eng", desc="", text=[value]))

    def _set_id3_cover(self, tags: ID3, cover: QPixmap | None) -> None:
        tags.delall("APIC")
        if cover is None or cover.isNull():
            return

        data = self._pixmap_bytes(cover)
        if not data:
            return

        tags.add(APIC(encoding=3, mime="image/png", type=3, desc="", data=data))

    def _set_flac_cover(self, audio: FLAC, cover: QPixmap | None) -> None:
        if cover is None or cover.isNull():
            return

        data = self._pixmap_bytes(cover)
        if not data:
            return

        picture = Picture()
        picture.type = 3
        picture.mime = "image/png"
        picture.desc = ""
        picture.data = data
        picture.width = cover.width()
        picture.height = cover.height()
        picture.depth = 32
        picture.colors = 0
        audio.add_picture(picture)

    def _set_vorbis_cover(self, tags, cover: QPixmap | None) -> None:
        if "METADATA_BLOCK_PICTURE" in tags:
            del tags["METADATA_BLOCK_PICTURE"]
        if cover is None or cover.isNull():
            return

        data = self._pixmap_bytes(cover)
        if not data:
            return

        picture = Picture()
        picture.type = 3
        picture.mime = "image/png"
        picture.desc = ""
        picture.data = data
        picture.width = cover.width()
        picture.height = cover.height()
        picture.depth = 32
        picture.colors = 0
        tags["METADATA_BLOCK_PICTURE"] = [base64.b64encode(picture.write()).decode("ascii")]

    def _set_mp4_cover(self, tags, cover: QPixmap | None) -> None:
        if "covr" in tags:
            del tags["covr"]
        if cover is None or cover.isNull():
            return

        data = self._pixmap_bytes(cover)
        if not data:
            return

        tags["covr"] = [MP4Cover(data, imageformat=MP4Cover.FORMAT_PNG)]

    def _id3_text(self, tags: ID3, *frame_ids: str) -> str:
        for frame_id in frame_ids:
            frames = tags.getall(frame_id)
            if not frames:
                continue
            frame = frames[0]
            if hasattr(frame, "text") and frame.text:
                return str(frame.text[0]).strip()
            value = str(frame).strip()
            if value:
                return value
        return ""

    def _id3_comment(self, tags: ID3) -> str:
        for frame in tags.getall("COMM"):
            text = getattr(frame, "text", None)
            if text:
                return str(text[0]).strip()
        return ""

    def _id3_cover(self, tags: ID3) -> QPixmap | None:
        frames = tags.getall("APIC")
        if not frames:
            return None
        frame = frames[0]
        data = getattr(frame, "data", b"")
        return self._pixmap_from_bytes(data)

    def _tag_text(self, tags, *keys: str) -> str:
        value = self._first_tag_value(tags, *keys)
        if value is None:
            return ""
        if isinstance(value, (list, tuple)):
            value = value[0] if value else ""
        return str(value).strip()

    def _first_tag_value(self, tags, *keys: str):
        for key in keys:
            if key in tags:
                value = tags[key]
                if isinstance(value, (list, tuple)):
                    return value[0] if value else None
                return value
        return None

    def _mp4_text(self, tags, key: str) -> str:
        value = tags.get(key)
        if not value:
            return ""
        if isinstance(value, (list, tuple)):
            value = value[0] if value else ""
        return str(value).strip()

    def _mp4_pair(self, tags, key: str) -> str:
        value = tags.get(key)
        if not value:
            return ""
        if isinstance(value, (list, tuple)) and value:
            first = value[0]
            if isinstance(first, (list, tuple)) and first:
                return str(first[0]).strip() if first[0] else ""
        return ""

    def _mp4_int(self, tags, key: str) -> str:
        value = tags.get(key)
        if not value:
            return ""
        if isinstance(value, (list, tuple)) and value:
            return str(value[0]).strip()
        return str(value).strip()

    def _int_or_none(self, value: str) -> int | None:
        value = value.strip()
        if not value:
            return None
        try:
            return int(value)
        except ValueError:
            return None

    def _pixmap_bytes(self, pixmap: QPixmap) -> bytes:
        from PySide6.QtCore import QBuffer, QIODevice

        data = bytearray()
        buffer = QBuffer()
        buffer.open(QIODevice.OpenModeFlag.WriteOnly)
        pixmap.save(buffer, "PNG")
        data.extend(buffer.data())
        buffer.close()
        return bytes(data)

    def _pixmap_from_bytes(self, data: bytes) -> QPixmap | None:
        if not data:
            return None
        pixmap = QPixmap()
        if pixmap.loadFromData(data):
            return pixmap
        return None

    def _pixmap_from_picture(self, picture: Picture) -> QPixmap | None:
        return self._pixmap_from_bytes(picture.data)

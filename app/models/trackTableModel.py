from __future__ import annotations

from typing import Sequence

from PySide6.QtCore import QAbstractTableModel, QFileInfo, QModelIndex, Qt
from PySide6.QtGui import QIcon, QPixmap

from app.models.track import Track


class TrackTableModel(QAbstractTableModel):
    CoverColumn = 0
    NameColumn = 1
    ArtistColumn = 2
    AlbumColumn = 3
    YearColumn = 4
    PathColumn = 5
    ColumnCount = 6

    def __init__(self, parent=None):
        super().__init__(parent)
        self._tracks: list[Track] = []

    def rowCount(self, parent: QModelIndex = QModelIndex()) -> int:
        return 0 if parent.isValid() else len(self._tracks)

    def columnCount(self, parent: QModelIndex = QModelIndex()) -> int:
        return 0 if parent.isValid() else self.ColumnCount

    def data(self, index: QModelIndex, role: int = Qt.ItemDataRole.DisplayRole):
        if not index.isValid():
            return None

        row = index.row()
        if row < 0 or row >= len(self._tracks):
            return None

        track = self._tracks[row]

        if role == Qt.ItemDataRole.DecorationRole and index.column() == self.CoverColumn:
            cover = self.coverForTrack(track)
            return QIcon(cover)

        if role == Qt.ItemDataRole.DisplayRole:
            if index.column() == self.CoverColumn:
                return None
            if index.column() == self.NameColumn:
                return self.displayNameForTrack(track)
            if index.column() == self.ArtistColumn:
                return track.artist
            if index.column() == self.AlbumColumn:
                return track.album
            if index.column() == self.YearColumn:
                return track.year
            if index.column() == self.PathColumn:
                return track.filePath
            return None

        if role == Qt.ItemDataRole.TextAlignmentRole and index.column() == self.YearColumn:
            return Qt.AlignmentFlag.AlignCenter

        return None

    def headerData(
        self,
        section: int,
        orientation: Qt.Orientation,
        role: int = Qt.ItemDataRole.DisplayRole,
    ):
        if orientation != Qt.Orientation.Horizontal or role != Qt.ItemDataRole.DisplayRole:
            return None

        if section == self.CoverColumn:
            return self.tr("Cover")
        if section == self.NameColumn:
            return self.tr("Name")
        if section == self.ArtistColumn:
            return self.tr("Artist")
        if section == self.AlbumColumn:
            return self.tr("Album")
        if section == self.YearColumn:
            return self.tr("Year")
        if section == self.PathColumn:
            return self.tr("File")
        return None

    def flags(self, index: QModelIndex):
        if not index.isValid():
            return Qt.ItemFlag.NoItemFlags
        return Qt.ItemFlag.ItemIsSelectable | Qt.ItemFlag.ItemIsEnabled

    def setTracks(self, tracks: Sequence[Track]) -> None:
        self.beginResetModel()
        self._tracks = list(tracks)
        self.endResetModel()

    def addTrack(self, track: Track) -> None:
        row = len(self._tracks)
        self.beginInsertRows(QModelIndex(), row, row)
        self._tracks.append(track)
        self.endInsertRows()

    def trackAt(self, row: int) -> Track | None:
        if row < 0 or row >= len(self._tracks):
            return None
        return self._tracks[row]

    def updateTrack(self, row: int, track: Track) -> None:
        if row < 0 or row >= len(self._tracks):
            return

        self._tracks[row] = track
        top_left = self.index(row, 0)
        bottom_right = self.index(row, self.ColumnCount - 1)
        self.dataChanged.emit(top_left, bottom_right, [Qt.ItemDataRole.DisplayRole, Qt.ItemDataRole.DecorationRole])

    def removeTrack(self, row: int) -> None:
        if row < 0 or row >= len(self._tracks):
            return

        self.beginRemoveRows(QModelIndex(), row, row)
        self._tracks.pop(row)
        self.endRemoveRows()

    def coverForTrack(self, track: Track) -> QPixmap:
        if track.coverArt is not None and not track.coverArt.isNull():
            return track.coverArt.scaled(
                48,
                48,
                Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                Qt.TransformationMode.SmoothTransformation,
            )
        return QPixmap()

    def displayNameForTrack(self, track: Track) -> str:
        if track.title:
            return track.title
        return QFileInfo(track.filePath).baseName()

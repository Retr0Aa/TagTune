from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPixmap
from PySide6.QtWidgets import (
    QFrame,
    QFileDialog,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QSizePolicy,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from app.models.track import Track


def _make_default_cover() -> QPixmap:
    pixmap = QPixmap(160, 160)
    pixmap.fill(QColor(46, 52, 64))

    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.setPen(Qt.GlobalColor.white)
    font = painter.font()
    font.setPointSize(14)
    font.setBold(True)
    painter.setFont(font)
    painter.drawText(pixmap.rect(), Qt.AlignmentFlag.AlignCenter, "No Cover")
    painter.end()
    return pixmap


class TagEditorWidget(QWidget):
    saveRequested = Signal(object)
    coverRequested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._track = Track()
        self._last_layout_mode = -1

        root_layout = QHBoxLayout(self)

        cover_column = QVBoxLayout()
        self._cover_label = QLabel()
        self._cover_label.setFixedSize(160, 160)
        self._cover_label.setFrameShape(QFrame.Shape.StyledPanel)
        self._cover_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._set_empty_cover()

        self._file_label = QLabel("No track selected")
        self._file_label.setWordWrap(True)

        self._cover_button = QPushButton("Load Cover")
        self._remove_cover_button = QPushButton("Remove Cover")
        self._remove_cover_button.setEnabled(False)

        cover_column.addWidget(self._cover_label, 0, Qt.AlignmentFlag.AlignLeft)
        cover_column.addWidget(self._file_label)
        cover_column.addWidget(self._cover_button)
        cover_column.addWidget(self._remove_cover_button)
        cover_column.addStretch()

        self._fields_container = QWidget()
        self._fields_layout = QGridLayout(self._fields_container)
        self._fields_layout.setContentsMargins(0, 0, 0, 0)
        self._fields_layout.setHorizontalSpacing(16)
        self._fields_layout.setVerticalSpacing(10)

        self._title_edit = QLineEdit()
        self._artist_edit = QLineEdit()
        self._album_edit = QLineEdit()
        self._album_artist_edit = QLineEdit()
        self._genre_edit = QLineEdit()
        self._comment_edit = QLineEdit()
        self._composer_edit = QLineEdit()

        self._year_spin = QSpinBox()
        self._year_spin.setRange(0, 9999)
        self._track_spin = QSpinBox()
        self._track_spin.setRange(0, 999)
        self._disc_spin = QSpinBox()
        self._disc_spin.setRange(0, 99)
        self._bpm_spin = QSpinBox()
        self._bpm_spin.setRange(0, 999)

        for field, minimum_width in (
            (self._title_edit, 320),
            (self._artist_edit, 260),
            (self._album_edit, 260),
            (self._album_artist_edit, 260),
            (self._genre_edit, 220),
            (self._comment_edit, 320),
            (self._composer_edit, 260),
        ):
            field.setMinimumWidth(minimum_width)
            field.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        for spin in (self._year_spin, self._track_spin, self._disc_spin, self._bpm_spin):
            spin.setMinimumWidth(120)

        self._save_button = QPushButton("Save Tags")

        editor_column = QVBoxLayout()
        editor_column.addWidget(self._fields_container)
        editor_column.addWidget(self._save_button)
        editor_column.addStretch()

        root_layout.addLayout(cover_column, 1)
        root_layout.addLayout(editor_column, 3)

        self._save_button.clicked.connect(self._on_save_clicked)
        self._cover_button.clicked.connect(self._on_load_cover_clicked)
        self._remove_cover_button.clicked.connect(self._on_remove_cover_clicked)

        self._rebuild_field_layout()

    def setTrack(self, track: Track) -> None:
        self._track = track
        self._apply_track(track)

    def track(self) -> Track:
        return self._collect_track()

    def clearTrack(self) -> None:
        self._track = Track()
        self._file_label.setText("No track selected")
        self._title_edit.clear()
        self._artist_edit.clear()
        self._album_edit.clear()
        self._album_artist_edit.clear()
        self._genre_edit.clear()
        self._comment_edit.clear()
        self._composer_edit.clear()
        self._year_spin.setValue(0)
        self._track_spin.setValue(0)
        self._disc_spin.setValue(0)
        self._bpm_spin.setValue(0)
        self._set_empty_cover()
        self._remove_cover_button.setEnabled(False)

    def setEditingEnabled(self, enabled: bool) -> None:
        for widget in (
            self._file_label,
            self._cover_label,
            self._title_edit,
            self._artist_edit,
            self._album_edit,
            self._album_artist_edit,
            self._genre_edit,
            self._comment_edit,
            self._composer_edit,
            self._year_spin,
            self._track_spin,
            self._disc_spin,
            self._bpm_spin,
            self._save_button,
            self._cover_button,
            self._remove_cover_button,
        ):
            widget.setEnabled(enabled)

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._rebuild_field_layout()

    def _on_save_clicked(self) -> None:
        self.saveRequested.emit(self._collect_track())

    def _on_load_cover_clicked(self) -> None:
        file_name, _ = QFileDialog.getOpenFileName(
            self,
            "Select Cover Art",
            "",
            "Images (*.png *.jpg *.jpeg *.bmp *.webp)",
        )
        if not file_name:
            return

        pixmap = QPixmap(file_name)
        if pixmap.isNull():
            return

        self._track.coverArt = pixmap
        self._set_cover_pixmap(pixmap)
        self._remove_cover_button.setEnabled(True)
        self.coverRequested.emit()

    def _on_remove_cover_clicked(self) -> None:
        self._track.coverArt = None
        self._set_empty_cover()
        self._remove_cover_button.setEnabled(False)
        self.coverRequested.emit()

    def _apply_track(self, track: Track) -> None:
        self._file_label.setText(track.filePath or "No track selected")
        self._title_edit.setText(track.title)
        self._artist_edit.setText(track.artist)
        self._album_edit.setText(track.album)
        self._album_artist_edit.setText(track.albumArtist)
        self._genre_edit.setText(track.genre)
        self._comment_edit.setText(track.comment)
        self._composer_edit.setText(track.composer)
        self._year_spin.setValue(int(track.year or 0))
        self._track_spin.setValue(int(track.trackNumber or 0))
        self._disc_spin.setValue(int(track.discNumber or 0))
        self._bpm_spin.setValue(int(track.bpm or 0))

        if track.coverArt is None or track.coverArt.isNull():
            self._set_empty_cover()
            self._remove_cover_button.setEnabled(False)
        else:
            self._set_cover_pixmap(track.coverArt)
            self._remove_cover_button.setEnabled(True)

    def _collect_track(self) -> Track:
        updated = self._track
        updated.title = self._title_edit.text().strip()
        updated.artist = self._artist_edit.text().strip()
        updated.album = self._album_edit.text().strip()
        updated.albumArtist = self._album_artist_edit.text().strip()
        updated.genre = self._genre_edit.text().strip()
        updated.comment = self._comment_edit.text().strip()
        updated.composer = self._composer_edit.text().strip()
        updated.year = str(self._year_spin.value())
        updated.trackNumber = str(self._track_spin.value())
        updated.discNumber = str(self._disc_spin.value())
        updated.bpm = str(self._bpm_spin.value())
        return updated

    def _set_cover_pixmap(self, pixmap: QPixmap) -> None:
        if pixmap.isNull():
            self._set_empty_cover()
            return

        self._cover_label.setPixmap(
            pixmap.scaled(
                self._cover_label.size(),
                Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                Qt.TransformationMode.SmoothTransformation,
            )
        )

    def _set_empty_cover(self) -> None:
        self._cover_label.setPixmap(
            _make_default_cover().scaled(
                self._cover_label.size(),
                Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                Qt.TransformationMode.SmoothTransformation,
            )
        )

    def _rebuild_field_layout(self) -> None:
        width = self._fields_container.width()
        layout_mode = 2 if width >= 700 else 1
        if layout_mode == self._last_layout_mode:
            return
        self._last_layout_mode = layout_mode

        while (item := self._fields_layout.takeAt(0)) is not None:
            widget = item.widget()
            if widget is not None and isinstance(widget, QLabel):
                widget.deleteLater()

        rows = [
            ("Title", self._title_edit),
            ("Artist", self._artist_edit),
            ("Album", self._album_edit),
            ("Album Artist", self._album_artist_edit),
            ("Genre", self._genre_edit),
            ("Comment", self._comment_edit),
            ("Composer", self._composer_edit),
            ("Year", self._year_spin),
            ("Track #", self._track_spin),
            ("Disc #", self._disc_spin),
            ("BPM", self._bpm_spin),
        ]

        row_count = (len(rows) + 1) // 2 if layout_mode == 2 else len(rows)
        for index, (label_text, field) in enumerate(rows):
            column_group = index // row_count if layout_mode == 2 else 0
            row = index % row_count if layout_mode == 2 else index
            base_column = column_group * 2

            label = QLabel(label_text)
            label.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
            self._fields_layout.addWidget(label, row, base_column)
            self._fields_layout.addWidget(field, row, base_column + 1)

        self._fields_layout.setColumnStretch(0, 0)
        self._fields_layout.setColumnStretch(1, 1)
        self._fields_layout.setColumnStretch(2, 0)
        self._fields_layout.setColumnStretch(3, 1)

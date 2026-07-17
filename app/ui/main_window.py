from __future__ import annotations

import sys

from PySide6.QtCore import QFileInfo, QProcess, QPoint, QSettings, QSize, Qt, QUrl, Signal
from PySide6.QtGui import QAction, QDesktopServices
from PySide6.QtWidgets import (
    QAbstractItemView,
    QApplication,
    QFileDialog,
    QHeaderView,
    QLabel,
    QMainWindow,
    QMenu,
    QMessageBox,
    QStatusBar,
    QTableView,
    QToolBar,
    QSplitter,
    QVBoxLayout,
    QWidget,
)

from app.models.metadata_service import MetadataService
from app.models.track import Track
from app.models.trackTableModel import TrackTableModel
from app.ui.icon_helper import app_icon
from app.ui.tag_editor_widget import TagEditorWidget


class TrackTableView(QTableView):
    filesDropped = Signal(list)

    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
            return
        super().dragEnterEvent(event)

    def dragMoveEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
            return
        super().dragMoveEvent(event)

    def dropEvent(self, event):
        if not event.mimeData().hasUrls():
            super().dropEvent(event)
            return

        paths = [url.toLocalFile() for url in event.mimeData().urls() if url.isLocalFile()]
        if paths:
            self.filesDropped.emit(paths)
        event.acceptProposedAction()


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self._metadata_service = MetadataService()
        self._model = TrackTableModel(self)
        self._loaded_paths: set[str] = set()

        self._table = TrackTableView(self)
        self._editor = TagEditorWidget(self)
        self._recent_menu: QMenu | None = None
        self._count_label = QLabel("0 tracks")
        self._selection_label = QLabel("No track selected")

        self._open_action = QAction("Open Files...", self)
        self._open_folder_action = QAction("Open Folder...", self)
        self._clear_action = QAction("Clear Selection", self)
        self._remove_action = QAction("Remove", self)
        self._show_in_finder_action = QAction("Show in File Manager", self)
        self._quit_action = QAction("Quit", self)
        self._about_action = QAction("About TagTune", self)

        icon = app_icon()
        self.setWindowIcon(icon)
        self.setWindowTitle("TagTune")
        self.resize(1300, 900)
        self.setAcceptDrops(True)

        self._setup_ui()
        self._setup_actions()
        self._setup_toolbar()
        self._setup_menus()
        self._update_activity_bar()

    def _setup_ui(self) -> None:
        central = QWidget(self)
        layout = QVBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        splitter = QSplitter(Qt.Orientation.Vertical, central)

        self._table.setModel(self._model)
        self._table.setAcceptDrops(True)
        self._table.viewport().setAcceptDrops(True)
        self._table.setDragEnabled(False)
        self._table.setDragDropMode(QAbstractItemView.DragDropMode.DropOnly)
        self._table.setDropIndicatorShown(True)
        self._table.setDefaultDropAction(Qt.DropAction.CopyAction)
        self._table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self._table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self._table.setAlternatingRowColors(True)
        self._table.setSortingEnabled(False)
        self._table.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self._table.customContextMenuRequested.connect(self._show_context_menu)
        self._table.filesDropped.connect(self._handle_dropped_paths)

        header = self._table.horizontalHeader()
        self._table.setIconSize(QSize(32, 32))
        self._table.verticalHeader().setVisible(False)
        self._table.verticalHeader().setDefaultSectionSize(40)
        header.setStretchLastSection(True)
        header.setSectionResizeMode(TrackTableModel.CoverColumn, QHeaderView.ResizeMode.Fixed)
        header.resizeSection(TrackTableModel.CoverColumn, 48)
        header.setSectionResizeMode(TrackTableModel.NameColumn, QHeaderView.ResizeMode.Interactive)
        header.setSectionResizeMode(TrackTableModel.ArtistColumn, QHeaderView.ResizeMode.Interactive)
        header.setSectionResizeMode(TrackTableModel.AlbumColumn, QHeaderView.ResizeMode.Interactive)
        header.setSectionResizeMode(TrackTableModel.YearColumn, QHeaderView.ResizeMode.Interactive)
        header.setSectionResizeMode(TrackTableModel.PathColumn, QHeaderView.ResizeMode.Interactive)
        header.setMinimumSectionSize(40)

        self._editor.setEditingEnabled(False)
        self._editor.saveRequested.connect(self._on_save_requested)

        splitter.addWidget(self._table)
        splitter.addWidget(self._editor)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)
        splitter.setChildrenCollapsible(False)

        layout.addWidget(splitter)
        self.setCentralWidget(central)

        self._table.selectionModel().currentRowChanged.connect(self._on_selection_changed)
        self._table.doubleClicked.connect(lambda _index: self._show_selected_in_file_manager())

        status = QStatusBar(self)
        status.addPermanentWidget(self._count_label)
        status.addPermanentWidget(self._selection_label)
        self.setStatusBar(status)
        self.statusBar().showMessage("Load audio files to begin.")

    def _setup_actions(self) -> None:
        self._open_action.triggered.connect(self._load_files)
        self._open_folder_action.triggered.connect(self._load_folder_dialog)
        self._clear_action.triggered.connect(self._clear_selection)
        self._remove_action.triggered.connect(self._remove_selected_track)
        self._show_in_finder_action.triggered.connect(self._show_selected_in_file_manager)
        self._quit_action.setMenuRole(QAction.MenuRole.QuitRole)
        self._quit_action.triggered.connect(QApplication.instance().quit)
        self._about_action.setMenuRole(QAction.MenuRole.AboutRole)
        self._about_action.triggered.connect(self._show_about)

    def _setup_toolbar(self) -> None:
        toolbar = QToolBar("Main", self)
        toolbar.setMovable(False)
        toolbar.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonTextBesideIcon)
        toolbar.addAction(self._open_action)
        toolbar.addAction(self._open_folder_action)
        toolbar.addSeparator()
        toolbar.addAction(self._clear_action)
        toolbar.addAction(self._remove_action)
        toolbar.addAction(self._show_in_finder_action)
        self.addToolBar(toolbar)

    def _setup_menus(self) -> None:
        file_menu = self.menuBar().addMenu("&File")
        file_menu.addAction(self._open_action)
        file_menu.addAction(self._open_folder_action)
        file_menu.addSeparator()
        file_menu.addAction(self._clear_action)
        file_menu.addAction(self._remove_action)
        file_menu.addAction(self._show_in_finder_action)
        file_menu.addSeparator()
        self._recent_menu = QMenu("Open Recent", self)
        file_menu.addMenu(self._recent_menu)
        file_menu.addSeparator()
        file_menu.addAction(self._quit_action)
        self._update_recent_menu()

        help_menu = self.menuBar().addMenu("&Help")
        help_menu.addAction(self._about_action)

    def _show_about(self) -> None:
        QMessageBox.about(
            self,
            "About TagTune",
            """
            <h2>TagTune</h2>
            <p><b>Version:</b> 1.0</p>
            <p>A music metadata editor built with Qt.</p>
            <p>© 2026 Retr0A</p>
            """,
        )

    def _show_message(self, message: str, timeout_ms: int = 3000) -> None:
        self.statusBar().showMessage(message, timeout_ms)

    def _update_activity_bar(self) -> None:
        self._count_label.setText(f"{self._model.rowCount()} tracks")
        current = self._table.currentIndex()
        if current.isValid():
            track = self._model.trackAt(current.row())
            if track is not None:
                self._selection_label.setText(self._selection_text(track))
                return
        self._selection_label.setText("No track selected")

    def _selection_text(self, track: Track) -> str:
        if track.title.strip():
            return track.title.strip()
        return QFileInfo(track.filePath).fileName() or "No track selected"

    def _normalize_path(self, path: str) -> str:
        info = QFileInfo(path)
        canonical = info.canonicalFilePath()
        return canonical or info.absoluteFilePath()

    def _add_to_recent(self, path: str) -> None:
        settings = QSettings()
        recent = settings.value("recentPaths", [], type=list)
        normalized = self._normalize_path(path)
        recent = [item for item in recent if item != normalized]
        recent.insert(0, normalized)
        settings.setValue("recentPaths", recent[:12])
        self._update_recent_menu()

    def _remove_from_recent(self, path: str) -> None:
        settings = QSettings()
        recent = settings.value("recentPaths", [], type=list)
        normalized = self._normalize_path(path)
        recent = [item for item in recent if item != normalized]
        settings.setValue("recentPaths", recent)
        self._update_recent_menu()

    def _update_recent_menu(self) -> None:
        if self._recent_menu is None:
            return

        self._recent_menu.clear()
        settings = QSettings()
        recent = settings.value("recentPaths", [], type=list)
        if not recent:
            self._recent_menu.setEnabled(False)
            return

        self._recent_menu.setEnabled(True)
        for path in recent:
            action = self._recent_menu.addAction(path)
            action.triggered.connect(lambda checked=False, p=path: self._open_recent_path(p))

    def _open_recent_path(self, path: str) -> None:
        info = QFileInfo(path)
        if not info.exists():
            QMessageBox.warning(self, "TagTune", f"That recent location no longer exists:\n{path}")
            self._remove_from_recent(path)
            return

        if info.isDir():
            self._load_folder(path)
            return

        if info.isFile():
            self._load_paths([path])

    def _load_folder_dialog(self) -> None:
        folder = QFileDialog.getExistingDirectory(self, "Select Music Folder")
        if not folder:
            return

        self._add_to_recent(folder)
        self._load_folder(folder)

    def _load_files(self) -> None:
        files, _ = QFileDialog.getOpenFileNames(
            self,
            "Open Audio Files",
            "",
            "Audio Files (*.mp3 *.flac *.wav *.ogg *.m4a *.aac *.mp4);;All Files (*.*)",
        )
        if not files:
            return

        self._add_to_recent(QFileInfo(files[0]).absolutePath())
        self._load_paths(files)

    def _load_folder(self, folder_path: str) -> None:
        files = self._metadata_service.scan_folder(folder_path)
        if not files:
            QMessageBox.information(self, "TagTune", f"No audio files found in {folder_path}")
            return

        added = self._load_paths(files)
        self._show_message(f"Loaded {added} file(s) from folder.", 3000)

    def _load_paths(self, paths: list[str]) -> int:
        to_load: list[str] = []
        for path in paths:
            info = QFileInfo(path)
            if not info.exists() or not info.isFile() or not self._metadata_service.is_audio_file(path):
                continue

            normalized = self._normalize_path(path)
            if normalized not in self._loaded_paths:
                to_load.append(path)

        if not to_load:
            self._show_message("No new audio files to load.", 2000)
            return 0

        loaded_tracks = self._metadata_service.load_tracks(to_load)
        added = 0
        for track in loaded_tracks:
            normalized = self._normalize_path(track.filePath)
            if normalized in self._loaded_paths:
                continue

            self._model.addTrack(track)
            self._loaded_paths.add(normalized)
            added += 1

        if added > 0 and not self._table.currentIndex().isValid():
            self._table.selectRow(0)
        elif self._table.currentIndex().isValid():
            self._sync_editor_from_selection()

        self._update_activity_bar()
        self._show_message(f"Loaded {added} file(s).", 3000)
        return added

    def _current_track(self) -> Track | None:
        index = self._table.currentIndex()
        if not index.isValid():
            return None
        return self._model.trackAt(index.row())

    def _on_selection_changed(self, _current, _previous) -> None:
        self._sync_editor_from_selection()

    def _sync_editor_from_selection(self) -> None:
        current = self._table.currentIndex()
        if not current.isValid():
            self._editor.clearTrack()
            self._editor.setEditingEnabled(False)
            self._update_activity_bar()
            return

        track = self._model.trackAt(current.row())
        if track is None:
            self._editor.clearTrack()
            self._editor.setEditingEnabled(False)
            self._update_activity_bar()
            return

        self._editor.setTrack(track)
        self._editor.setEditingEnabled(True)
        self._update_activity_bar()

    def _clear_selection(self) -> None:
        self._table.clearSelection()
        self._editor.clearTrack()
        self._editor.setEditingEnabled(False)
        self._update_activity_bar()
        self._show_message("Selection cleared.", 2000)

    def _remove_selected_track(self) -> None:
        current = self._table.currentIndex()
        if not current.isValid():
            return

        row = current.row()
        track = self._model.trackAt(row)
        if track is None:
            return

        normalized = self._normalize_path(track.filePath)
        self._loaded_paths.discard(normalized)
        removed_name = QFileInfo(track.filePath).fileName() or self._selection_text(track)

        self._model.removeTrack(row)

        row_count = self._model.rowCount()
        if row_count > 0:
            self._table.selectRow(min(row, row_count - 1))
        else:
            self._table.clearSelection()
            self._editor.clearTrack()
            self._editor.setEditingEnabled(False)

        self._sync_editor_from_selection()
        self._show_message(f"Removed {removed_name}", 2000)

    def _on_save_requested(self, track: Track) -> None:
        current = self._table.currentIndex()
        if not current.isValid():
            return

        if not self._metadata_service.save_track(track):
            QMessageBox.warning(self, "TagTune", "Unable to save track.")
            return

        self._model.updateTrack(current.row(), track)
        self._editor.setTrack(track)
        display_name = track.title.strip() or QFileInfo(track.filePath).baseName()
        self._show_message(f"Saved tags for {display_name}", 3000)

    def _show_selected_in_file_manager(self) -> None:
        track = self._current_track()
        if track is None:
            return

        file_path = QFileInfo(track.filePath)
        if not file_path.exists():
            QMessageBox.warning(self, "TagTune", f"File no longer exists:\n{track.filePath}")
            return

        absolute_path = file_path.absoluteFilePath()
        if sys.platform == "darwin":
            QProcess.startDetached("open", ["-R", absolute_path])
        elif sys.platform.startswith("win"):
            QDesktopServices.openUrl(QUrl.fromLocalFile(file_path.absolutePath()))
        else:
            QDesktopServices.openUrl(QUrl.fromLocalFile(file_path.absolutePath()))

    def _show_context_menu(self, pos: QPoint) -> None:
        index = self._table.indexAt(pos)
        menu = QMenu(self)
        if index.isValid():
            menu.addAction(self._remove_action)
            menu.addAction(self._show_in_finder_action)
        menu.exec(self._table.viewport().mapToGlobal(pos))

    def _handle_dropped_paths(self, paths: list[str]) -> None:
        file_paths: list[str] = []
        for path in paths:
            info = QFileInfo(path)
            if info.isDir():
                self._add_to_recent(path)
                self._load_folder(path)
            elif info.isFile():
                file_paths.append(path)

        if file_paths:
            self._load_paths(file_paths)

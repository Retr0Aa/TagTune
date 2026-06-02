#include "MainWindow.h"

#include "MetadataService.h"
#include "TagEditorWidget.h"
#include "TrackTableModel.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDate>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QGuiApplication>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QPainter>
#include <QEvent>
#include <QMimeData>
#include <QSplitter>
#include <QItemSelectionModel>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QSettings>
#include <QDirIterator>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QSize>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#ifdef Q_OS_MAC
#include <QProcess>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_metadataService = new MetadataService;
    setupUi();
    setupActions();
    setupToolbar();
    setupMenuBar();
    updateSelectionState();
}

void MainWindow::setupUi()
{
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);

    auto *splitter = new QSplitter(Qt::Vertical, central);

    m_tableView = new QTableView(splitter);
    m_tableView->setAcceptDrops(true);
    m_tableView->viewport()->setAcceptDrops(true);
    m_tableView->setDragEnabled(false);
    m_tableView->setDragDropMode(QAbstractItemView::DropOnly);
    m_tableView->setDropIndicatorShown(true);
    m_tableView->setDefaultDropAction(Qt::CopyAction);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSortingEnabled(false);
    m_tableView->viewport()->installEventFilter(this);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QWidget::customContextMenuRequested, this, &MainWindow::onShowContextMenu);

    // Create and install the model before configuring header sections. If the
    // header is configured before a model is set, calls that resolve logical
    // <-> visual indices can return -1 and trigger assertions in Qt.
    m_model = new TrackTableModel(this);
    m_tableView->setModel(m_model);

    // Ensure icons (cover art) are visible at a reasonable size
    m_tableView->setIconSize(QSize(48, 48));
    m_tableView->verticalHeader()->setDefaultSectionSize(56);

    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_tableView->horizontalHeader()->resizeSection(0, 64);
    m_tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setMinimumSectionSize(48);

    m_editor = new TagEditorWidget(splitter);
    m_editor->setEditingEnabled(false);

    splitter->addWidget(m_tableView);
    splitter->addWidget(m_editor);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setChildrenCollapsible(false);

    layout->addWidget(splitter);
    setCentralWidget(central);

    connect(m_tableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::onSelectionChanged);
    connect(m_editor, &TagEditorWidget::saveRequested,
            this, &MainWindow::onSaveRequested);

    statusBar()->showMessage(tr("Load audio files to begin."));
    resize(1300, 900);
    setWindowTitle(tr("TagTune"));
}

void MainWindow::setupActions()
{
    m_openAction = new QAction(tr("Open Files..."), this);
    m_openFolderAction = new QAction(tr("Open Folder..."), this);
    m_saveAction = new QAction(tr("Save Current"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_clearAction = new QAction(tr("Clear Selection"), this);
    m_removeAction = new QAction(tr("Remove"), this);
    m_showInFinderAction = new QAction(tr("Show in Finder/Explorer"), this);
    m_recentMenu = nullptr;

    connect(m_openAction, &QAction::triggered, this, &MainWindow::loadFiles);
    connect(m_openFolderAction, &QAction::triggered, this, &MainWindow::loadFolderDialog);
    connect(m_saveAction, &QAction::triggered, this, [this]() {
        if (!m_tableView->currentIndex().isValid()) {
            return;
        }
        onSaveRequested(m_editor->track());
    });
    connect(m_clearAction, &QAction::triggered, this, [this]() {
        m_tableView->clearSelection();
        m_editor->clearTrack();
        updateSelectionState();
        statusBar()->showMessage(tr("Selection cleared."), 2000);
    });

    connect(m_removeAction, &QAction::triggered, this, &MainWindow::onRemoveRequested);
    connect(m_showInFinderAction, &QAction::triggered, this, &MainWindow::onShowInFinderRequested);
}

void MainWindow::setupToolbar()
{
#ifdef Q_OS_MAC
    // On macOS prefer the native menu bar for app-level actions (avoid duplicate top bars)
    // Keep a minimal in-window UI; do not add the top toolbar here so the global menu is primary.
#else
    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->addAction(m_openAction);
    toolbar->addAction(m_saveAction);
    toolbar->addAction(m_clearAction);
    toolbar->addAction(m_removeAction);
    toolbar->addSeparator();
#endif
}

void MainWindow::setupMenuBar()
{
    // Menu bar with File menu and Recent submenu (on macOS the QMainWindow menu bar will be native)
    QMenuBar *mb = menuBar();
    // On macOS use the native (global) menu bar
    mb->setNativeMenuBar(true);
    QMenu *fileMenu = mb->addMenu(tr("File"));

    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_openFolderAction);

    fileMenu->addSeparator();

    fileMenu->addAction(m_saveAction);

    fileMenu->addAction(m_clearAction);
    fileMenu->addAction(m_removeAction);
    fileMenu->addAction(m_showInFinderAction);

    fileMenu->addSeparator();

    // Recent menu (populated from QSettings)
    m_recentMenu = new QMenu(tr("Open Recent"), this);
    fileMenu->addMenu(m_recentMenu);
    updateRecentMenu();

    // Quit on Mac uses standard role
    QAction *quitAct = fileMenu->addAction(tr("Quit"));
    quitAct->setMenuRole(QAction::QuitRole);
    connect(quitAct, &QAction::triggered, qApp, &QCoreApplication::quit);
}

void MainWindow::updateRecentMenu()
{
    m_recentMenu->clear();
    QSettings s;
    const QStringList recent = s.value("recentPaths").toStringList();
    if (recent.isEmpty()) {
        m_recentMenu->setEnabled(false);
        return;
    }
    m_recentMenu->setEnabled(true);
    for (const QString &p : recent) {
        QAction *act = m_recentMenu->addAction(p);
        connect(act, &QAction::triggered, this, [this, p]() {
            QFileInfo fi(p);
            if (fi.exists() && fi.isDir()) {
                loadFolder(p);
            } else if (fi.exists() && fi.isFile()) {
                // Avoid duplicates when opening from recent
                QString key = fi.canonicalFilePath();
                if (key.isEmpty()) key = fi.absoluteFilePath();
                if (m_loadedPaths.contains(key)) return;
                const QVector<Track> loaded = m_metadataService->loadTracks(QStringList{p});
                for (const Track &t : loaded) {
                    QFileInfo tfi(t.filePath);
                    QString tkey = tfi.canonicalFilePath();
                    if (tkey.isEmpty()) tkey = tfi.absoluteFilePath();
                    if (m_loadedPaths.contains(tkey)) continue;
                    m_model->addTrack(t);
                    m_loadedPaths.insert(tkey);
                }
                if (!m_tableView->currentIndex().isValid()) m_tableView->selectRow(0);
            } else {
                QMessageBox::warning(this, tr("TagTune"), tr("That recent location no longer exists:\n%1").arg(p));
                removeFromRecent(p);
            }
        });
    }
}

void MainWindow::addToRecent(const QString &path)
{
    QSettings s;
    QStringList recent = s.value("recentPaths").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 12) recent.removeLast();
    s.setValue("recentPaths", recent);
    updateRecentMenu();
}

void MainWindow::removeFromRecent(const QString &path)
{
    QSettings s;
    QStringList recent = s.value("recentPaths").toStringList();
    recent.removeAll(path);
    s.setValue("recentPaths", recent);
    updateRecentMenu();
}

static bool isAudioFileName(const QString &name)
{
    const QString n = name.toLower();
    return n.endsWith(".mp3") || n.endsWith(".m4a") || n.endsWith(".mp4") || n.endsWith(".flac") || n.endsWith(".wav") || n.endsWith(".ogg") || n.endsWith(".aac");
}

void MainWindow::loadFolderDialog()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Select Music Folder"));
    if (folder.isEmpty()) return;
    addToRecent(folder);
    loadFolder(folder);
}

void MainWindow::loadFolder(const QString &folderPath)
{
    QVector<QString> files;
    QDirIterator it(folderPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        if (isAudioFileName(f)) files.append(f);
    }

    if (files.isEmpty()) {
        QMessageBox::information(this, tr("TagTune"), tr("No audio files found in %1").arg(folderPath));
        return;
    }

    QStringList qsl;
    for (const QString &s : files) qsl.append(s);
    const QVector<Track> loadedTracks = m_metadataService->loadTracks(qsl);
    int added = 0;
    for (const Track &track : loadedTracks) {
        QFileInfo tfi(track.filePath);
        QString key = tfi.canonicalFilePath();
        if (key.isEmpty()) key = tfi.absoluteFilePath();
        if (m_loadedPaths.contains(key)) continue;
        m_model->addTrack(track);
        m_loadedPaths.insert(key);
        ++added;
    }
    if (!m_tableView->currentIndex().isValid() && added > 0) m_tableView->selectRow(0);
    refreshEditorFromSelection();
    statusBar()->showMessage(tr("Loaded %1 file(s) from folder.").arg(added), 3000);
}

// Demo data removed: application no longer seeds sample tracks.

void MainWindow::loadFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Open Audio Files"),
        QString(),
        tr("Audio Files (*.mp3 *.flac *.wav *.ogg *.m4a *.aac);;All Files (*.*)")
    );

    if (files.isEmpty()) {
        return;
    }
    addToRecent(QFileInfo(files.first()).absolutePath());
    loadPaths(files);
}

void MainWindow::loadPaths(const QStringList &paths)
{
    QStringList toLoad;
    toLoad.reserve(paths.size());
    for (const QString &path : paths) {
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile() || !isAudioFileName(fi.fileName())) {
            continue;
        }
        QString key = fi.canonicalFilePath();
        if (key.isEmpty()) key = fi.absoluteFilePath();
        if (!m_loadedPaths.contains(key)) {
            toLoad.append(path);
        }
    }

    if (toLoad.isEmpty()) {
        statusBar()->showMessage(tr("No new audio files to load."), 2000);
        return;
    }

    const QVector<Track> loadedTracks = m_metadataService->loadTracks(toLoad);
    int added = 0;
    for (const Track &track : loadedTracks) {
        QFileInfo tfi(track.filePath);
        QString key = tfi.canonicalFilePath();
        if (key.isEmpty()) key = tfi.absoluteFilePath();
        if (m_loadedPaths.contains(key)) continue;
        m_model->addTrack(track);
        m_loadedPaths.insert(key);
        ++added;
    }

    if (!m_tableView->currentIndex().isValid() && added > 0) {
        m_tableView->selectRow(0);
    }

    refreshEditorFromSelection();
    statusBar()->showMessage(tr("Loaded %1 file(s)." ).arg(added), 3000);
}

void MainWindow::onSelectionChanged()
{
    refreshEditorFromSelection();
}

void MainWindow::updateSelectionState()
{
    const bool hasSelection = m_tableView && m_tableView->currentIndex().isValid();
    m_editor->setEditingEnabled(hasSelection);
    m_saveAction->setEnabled(hasSelection);
    m_clearAction->setEnabled(hasSelection);
    m_removeAction->setEnabled(hasSelection);
    m_showInFinderAction->setEnabled(hasSelection);
}

void MainWindow::refreshEditorFromSelection()
{
    const QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid()) {
        m_editor->clearTrack();
        updateSelectionState();
        return;
    }

    m_editor->setTrack(m_model->trackAt(current.row()));
    updateSelectionState();
}

void MainWindow::onSaveRequested(const Track &track)
{
    const QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid()) {
        return;
    }

    QString errorMessage;
    if (!m_metadataService->saveTrack(track, &errorMessage)) {
        QMessageBox::warning(this, tr("TagTune"), errorMessage);
        return;
    }

    m_model->updateTrack(current.row(), track);
    statusBar()->showMessage(tr("Saved tags for %1").arg(track.title.isEmpty() ? QFileInfo(track.filePath).baseName() : track.title), 3000);
}

void MainWindow::onRemoveRequested()
{
    const QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid()) return;

    const int row = current.row();
    const Track t = m_model->trackAt(row);

    // Remove from loaded paths
    QFileInfo fi(t.filePath);
    QString key = fi.canonicalFilePath();
    if (key.isEmpty()) key = fi.absoluteFilePath();
    m_loadedPaths.remove(key);

    m_model->removeTrack(row);

    // update selection
    const int rows = m_model->rowCount();
    if (rows > 0) {
        m_tableView->selectRow(qMin(row, rows - 1));
    } else {
        m_editor->clearTrack();
    }
    refreshEditorFromSelection();
    updateSelectionState();

    statusBar()->showMessage(tr("Removed %1").arg(fi.fileName()), 2000);
}

QString MainWindow::selectedTrackPath() const
{
    const QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid()) return {};
    return m_model->trackAt(current.row()).filePath;
}

void MainWindow::revealInFileManager(const QString &path) const
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        QMessageBox::warning(const_cast<MainWindow *>(this), tr("TagTune"), tr("File no longer exists:\n%1").arg(path));
        return;
    }

    const QString absolutePath = fi.absoluteFilePath();
#ifdef Q_OS_MAC
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), absolutePath});
#elif defined(Q_OS_WIN)
    const QString native = QDir::toNativeSeparators(absolutePath);
    const QString args = QStringLiteral("/select,\"") + native + QStringLiteral("\"");
    const std::wstring argsW = args.toStdWString();
    ShellExecuteW(nullptr, L"open", L"explorer.exe", argsW.c_str(), nullptr, SW_SHOW);
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
#endif
}

void MainWindow::onShowInFinderRequested()
{
    const QString path = selectedTrackPath();
    if (path.isEmpty()) return;
    revealInFileManager(path);
}

void MainWindow::onShowContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_tableView->indexAt(pos);
    QMenu menu(this);
    if (idx.isValid()) {
        menu.addAction(m_removeAction);
        menu.addAction(m_showInFinderAction);
    }
    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_tableView || watched == m_tableView->viewport()) && event) {
        if (event->type() == QEvent::DragEnter) {
            auto *dragEvent = static_cast<QDragEnterEvent *>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                dragEvent->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            auto *dragEvent = static_cast<QDragMoveEvent *>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                dragEvent->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto *dropEvent = static_cast<QDropEvent *>(event);
            QStringList paths;
            for (const QUrl &url : dropEvent->mimeData()->urls()) {
                if (url.isLocalFile()) paths.append(url.toLocalFile());
            }
            loadPaths(paths);
            dropEvent->acceptProposedAction();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

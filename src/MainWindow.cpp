#include "MainWindow.h"

#include "MetadataService.h"
#include "TagEditorWidget.h"
#include "TrackTableModel.h"

#include <QAction>
#include <QApplication>
#include <QDate>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QPainter>
#include <QSplitter>
#include <QItemSelectionModel>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QSettings>
#include <QDirIterator>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_metadataService = new MetadataService;
    setupUi();
    setupActions();
    setupToolbar();
    setupMenuBar();
}

void MainWindow::setupUi()
{
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);

    auto *splitter = new QSplitter(Qt::Vertical, central);

    m_tableView = new QTableView(splitter);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSortingEnabled(false);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_editor = new TagEditorWidget(splitter);

    splitter->addWidget(m_tableView);
    splitter->addWidget(m_editor);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setChildrenCollapsible(false);

    layout->addWidget(splitter);
    setCentralWidget(central);

    m_model = new TrackTableModel(this);
    m_tableView->setModel(m_model);

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
        statusBar()->showMessage(tr("Selection cleared."), 2000);
    });
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

    QAction *saveAct = new QAction(tr("Save"), this);
    saveAct->setShortcut(QKeySequence::Save);
    fileMenu->addAction(saveAct);
    // hook up to existing save action
    connect(saveAct, &QAction::triggered, m_saveAction, &QAction::trigger);

    fileMenu->addAction(m_clearAction);

    fileMenu->addSeparator();

    // Recent menu (populated from QSettings)
    m_recentMenu = new QMenu(tr("Open Recent"), this);
    fileMenu->addMenu(m_recentMenu);
    updateRecentMenu();

    // Quit on mac uses standard role
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
    // Remember parent folder of opened files as a recent location
    QFileInfo fi(files.first());
    if (fi.exists()) addToRecent(fi.absolutePath());
    // Filter out already-loaded files to avoid duplicates
    QStringList toLoad;
    toLoad.reserve(files.size());
    for (const QString &f : files) {
        QFileInfo fi2(f);
        QString key = fi2.canonicalFilePath();
        if (key.isEmpty()) key = fi2.absoluteFilePath();
        if (!m_loadedPaths.contains(key) && fi2.exists()) toLoad.append(f);
    }
    if (toLoad.isEmpty()) return;

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

void MainWindow::refreshEditorFromSelection()
{
    const QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid()) {
        m_editor->clearTrack();
        return;
    }

    m_editor->setTrack(m_model->trackAt(current.row()));
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




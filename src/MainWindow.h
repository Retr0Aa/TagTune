#pragma once

#include <QMainWindow>
#include <QSet>
#include <QVector>

#include "Track.h"

class QLineEdit;
class QAction;
class QSplitter;
class QTableView;
class QMenu;
class TrackTableModel;
class TagEditorWidget;
class MetadataService;
class QDropEvent;
class QDragEnterEvent;
class QMimeData;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void loadFiles();
    void onSelectionChanged();
    void onSaveRequested(const Track &track);
    void refreshEditorFromSelection();
    void onRemoveRequested();
    void onShowInFinderRequested();
    void onShowContextMenu(const QPoint &pos);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    void setupUi();
    void setupActions();
    void setupToolbar();
    void setupMenuBar();
    void updateSelectionState();
    void updateRecentMenu();
    void addToRecent(const QString &path);
    void removeFromRecent(const QString &path);
    void loadFolderDialog();
    void loadFolder(const QString &folderPath);
    void loadPaths(const QStringList &paths);
    void revealInFileManager(const QString &path) const;
    QString selectedTrackPath() const;

    QAction *m_openAction = nullptr;
    QAction *m_openFolderAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_clearAction = nullptr;
    QAction *m_removeAction = nullptr;
    QAction *m_showInFinderAction = nullptr;
    QMenu *m_recentMenu = nullptr;

    QTableView *m_tableView = nullptr;
    TrackTableModel *m_model = nullptr;
    TagEditorWidget *m_editor = nullptr;
    MetadataService *m_metadataService = nullptr;
    QSet<QString> m_loadedPaths;
};

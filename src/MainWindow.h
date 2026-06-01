#pragma once

#include <QMainWindow>
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
    void onShowContextMenu(const QPoint &pos);

private:
    void setupUi();
    void setupActions();
    void setupToolbar();
    void setupMenuBar();
    void updateRecentMenu();
    void addToRecent(const QString &path);
    void removeFromRecent(const QString &path);
    void loadFolderDialog();
    void loadFolder(const QString &folderPath);

    QAction *m_openAction = nullptr;
    QAction *m_openFolderAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_clearAction = nullptr;
    QAction *m_removeAction = nullptr;
    QMenu *m_recentMenu = nullptr;

    QTableView *m_tableView = nullptr;
    TrackTableModel *m_model = nullptr;
    TagEditorWidget *m_editor = nullptr;
    MetadataService *m_metadataService = nullptr;
    QSet<QString> m_loadedPaths;
};



#pragma once

#include <QWidget>

#include "Track.h"

class QGridLayout;
class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QPixmap;
class QResizeEvent;

class TagEditorWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TagEditorWidget(QWidget *parent = nullptr);

    void setTrack(const Track &track);
    Track track() const;
    void clearTrack();
    void setEditingEnabled(bool enabled);

signals:
    void saveRequested(const Track &track);
    void coverRequested();

private slots:
    void onSaveClicked();
    void onLoadCoverClicked();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyTrack(const Track &track);
    Track collectTrack() const;
    void setCoverPixmap(const QPixmap &pixmap);
    void setEmptyCover();
    void rebuildFieldLayout();

    QLabel *m_coverLabel = nullptr;
    QLabel *m_fileLabel = nullptr;
    QWidget *m_fieldsContainer = nullptr;
    QGridLayout *m_fieldsLayout = nullptr;

    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_artistEdit = nullptr;
    QLineEdit *m_albumEdit = nullptr;
    QLineEdit *m_albumArtistEdit = nullptr;
    QLineEdit *m_genreEdit = nullptr;
    QLineEdit *m_commentEdit = nullptr;
    QLineEdit *m_composerEdit = nullptr;
    QSpinBox *m_yearSpin = nullptr;
    QSpinBox *m_trackSpin = nullptr;
    QSpinBox *m_discSpin = nullptr;
    QSpinBox *m_bpmSpin = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_coverButton = nullptr;

    Track m_track;
    int m_lastLayoutMode = -1;
};

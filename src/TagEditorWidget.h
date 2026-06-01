#pragma once

#include <QWidget>

#include "Track.h"

class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QPixmap;

class TagEditorWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TagEditorWidget(QWidget *parent = nullptr);

    void setTrack(const Track &track);
    Track track() const;
    void clearTrack();

signals:
    void saveRequested(const Track &track);
    void coverRequested();

private slots:
    void onSaveClicked();
    void onLoadCoverClicked();

private:
    void applyTrack(const Track &track);
    Track collectTrack() const;
    void setCoverPixmap(const QPixmap &pixmap);
    void setEmptyCover();

    QLabel *m_coverLabel = nullptr;
    QLabel *m_fileLabel = nullptr;

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
};



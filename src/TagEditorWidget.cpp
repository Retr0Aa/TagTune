#include "TagEditorWidget.h"

#include <QFormLayout>
#include <QFrame>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QColor>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QPushButton>
#include <QSpinBox>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace
{
QPixmap makeDefaultCover()
{
    QPixmap pixmap(220, 220);
    pixmap.fill(QColor(46, 52, 64));

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(14);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QObject::tr("No Cover"));
    return pixmap;
}
}

TagEditorWidget::TagEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *rootLayout = new QHBoxLayout(this);

    auto *coverColumn = new QVBoxLayout;
    m_coverLabel = new QLabel;
    m_coverLabel->setFixedSize(220, 220);
    m_coverLabel->setFrameShape(QFrame::StyledPanel);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    setEmptyCover();

    m_fileLabel = new QLabel(tr("No track selected"));
    m_fileLabel->setWordWrap(true);

    m_coverButton = new QPushButton(tr("Load Cover"));

    coverColumn->addWidget(m_coverLabel, 0, Qt::AlignLeft);
    coverColumn->addWidget(m_fileLabel);
    coverColumn->addWidget(m_coverButton);
    coverColumn->addStretch();

    auto *formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setHorizontalSpacing(16);
    formLayout->setVerticalSpacing(10);

    m_titleEdit = new QLineEdit;
    m_artistEdit = new QLineEdit;
    m_albumEdit = new QLineEdit;
    m_albumArtistEdit = new QLineEdit;
    m_genreEdit = new QLineEdit;
    m_commentEdit = new QLineEdit;
    m_composerEdit = new QLineEdit;

    m_yearSpin = new QSpinBox;
    m_yearSpin->setRange(0, 9999);

    m_trackSpin = new QSpinBox;
    m_trackSpin->setRange(0, 999);

    m_discSpin = new QSpinBox;
    m_discSpin->setRange(0, 99);

    m_bpmSpin = new QSpinBox;
    m_bpmSpin->setRange(0, 999);

    const auto widenTextField = [](QLineEdit *edit, int minWidth) {
        edit->setMinimumWidth(minWidth);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    };

    widenTextField(m_titleEdit, 360);
    widenTextField(m_artistEdit, 300);
    widenTextField(m_albumEdit, 300);
    widenTextField(m_albumArtistEdit, 300);
    widenTextField(m_genreEdit, 240);
    widenTextField(m_commentEdit, 360);
    widenTextField(m_composerEdit, 300);

    m_yearSpin->setMinimumWidth(120);
    m_trackSpin->setMinimumWidth(120);
    m_discSpin->setMinimumWidth(120);
    m_bpmSpin->setMinimumWidth(120);

    formLayout->addRow(tr("Title"), m_titleEdit);
    formLayout->addRow(tr("Artist"), m_artistEdit);
    formLayout->addRow(tr("Album"), m_albumEdit);
    formLayout->addRow(tr("Album Artist"), m_albumArtistEdit);
    formLayout->addRow(tr("Genre"), m_genreEdit);
    formLayout->addRow(tr("Comment"), m_commentEdit);
    formLayout->addRow(tr("Composer"), m_composerEdit);
    formLayout->addRow(tr("Year"), m_yearSpin);
    formLayout->addRow(tr("Track #"), m_trackSpin);
    formLayout->addRow(tr("Disc #"), m_discSpin);
    formLayout->addRow(tr("BPM"), m_bpmSpin);

    m_saveButton = new QPushButton(tr("Save Tags"));

    auto *editorColumn = new QVBoxLayout;
    editorColumn->addLayout(formLayout);
    editorColumn->addWidget(m_saveButton);
    editorColumn->addStretch();

    rootLayout->addLayout(coverColumn, 1);
    rootLayout->addLayout(editorColumn, 3);

    connect(m_saveButton, &QPushButton::clicked, this, &TagEditorWidget::onSaveClicked);
    connect(m_coverButton, &QPushButton::clicked, this, &TagEditorWidget::onLoadCoverClicked);
}

void TagEditorWidget::setTrack(const Track &track)
{
    m_track = track;
    applyTrack(track);
}

Track TagEditorWidget::track() const
{
    return collectTrack();
}

void TagEditorWidget::clearTrack()
{
    m_track = {};
    m_fileLabel->setText(tr("No track selected"));
    m_titleEdit->clear();
    m_artistEdit->clear();
    m_albumEdit->clear();
    m_albumArtistEdit->clear();
    m_genreEdit->clear();
    m_commentEdit->clear();
    m_composerEdit->clear();
    m_yearSpin->setValue(0);
    m_trackSpin->setValue(0);
    m_discSpin->setValue(0);
    m_bpmSpin->setValue(0);
    setEmptyCover();
}

void TagEditorWidget::onSaveClicked()
{
    emit saveRequested(collectTrack());
}

void TagEditorWidget::onLoadCoverClicked()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Select Cover Art"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)")
    );

    if (fileName.isEmpty()) {
        return;
    }

    QPixmap pixmap(fileName);
    if (pixmap.isNull()) {
        return;
    }

    m_track.coverArt = pixmap;
    setCoverPixmap(pixmap);
    emit coverRequested();
}

void TagEditorWidget::applyTrack(const Track &track)
{
    m_fileLabel->setText(track.filePath.isEmpty() ? tr("No track selected") : track.filePath);
    m_titleEdit->setText(track.title);
    m_artistEdit->setText(track.artist);
    m_albumEdit->setText(track.album);
    m_albumArtistEdit->setText(track.albumArtist);
    m_genreEdit->setText(track.genre);
    m_commentEdit->setText(track.comment);
    m_composerEdit->setText(track.composer);
    m_yearSpin->setValue(track.year.toInt());
    m_trackSpin->setValue(track.trackNumber.toInt());
    m_discSpin->setValue(track.discNumber.toInt());
    m_bpmSpin->setValue(track.bpm.toInt());

    if (track.coverArt.isNull()) {
        setEmptyCover();
    } else {
        setCoverPixmap(track.coverArt);
    }
}

Track TagEditorWidget::collectTrack() const
{
    Track updated = m_track;
    updated.title = m_titleEdit->text().trimmed();
    updated.artist = m_artistEdit->text().trimmed();
    updated.album = m_albumEdit->text().trimmed();
    updated.albumArtist = m_albumArtistEdit->text().trimmed();
    updated.genre = m_genreEdit->text().trimmed();
    updated.comment = m_commentEdit->text().trimmed();
    updated.composer = m_composerEdit->text().trimmed();
    updated.year = QString::number(m_yearSpin->value());
    updated.trackNumber = QString::number(m_trackSpin->value());
    updated.discNumber = QString::number(m_discSpin->value());
    updated.bpm = QString::number(m_bpmSpin->value());
    return updated;
}

void TagEditorWidget::setCoverPixmap(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        setEmptyCover();
        return;
    }

    m_coverLabel->setPixmap(pixmap.scaled(
        m_coverLabel->size(),
        Qt::KeepAspectRatioByExpanding,
        Qt::SmoothTransformation));
}

void TagEditorWidget::setEmptyCover()
{
    m_coverLabel->setPixmap(makeDefaultCover().scaled(
        m_coverLabel->size(),
        Qt::KeepAspectRatioByExpanding,
        Qt::SmoothTransformation));
}




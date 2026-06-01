#pragma once

#include <QPixmap>
#include <QString>

struct Track
{
    QString filePath;
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString genre;
    QString comment;
    QString composer;
    QString year;
    QString trackNumber;
    QString discNumber;
    QString bpm;
    QPixmap coverArt;
};


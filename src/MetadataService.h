#pragma once

#include <QStringList>
#include <QVector>
#include "Track.h"

class QPixmap;

class MetadataService final
{
public:
    QVector<Track> loadTracks(const QStringList &filePaths) const;
    Track loadTrack(const QString &filePath) const;
    bool saveTrack(const Track &track, QString *errorMessage = nullptr) const;

private:
    static Track applyDefaults(const Track &track);
    // helper to normalize fields before writing
};




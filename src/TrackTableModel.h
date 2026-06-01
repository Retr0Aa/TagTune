#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "Track.h"

class TrackTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        CoverColumn = 0,
        NameColumn,
        ArtistColumn,
        AlbumColumn,
        YearColumn,
        PathColumn,
        ColumnCount
    };

    explicit TrackTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setTracks(QVector<Track> tracks);
    void addTrack(const Track &track);
    Track trackAt(int row) const;
    void updateTrack(int row, const Track &track);
    void removeTrack(int row);

private:
    QVector<Track> m_tracks;

    QPixmap coverForTrack(const Track &track) const;
    QString displayNameForTrack(const Track &track) const;
};


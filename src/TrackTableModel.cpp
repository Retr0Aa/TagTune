#include "TrackTableModel.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QFileInfo>
#include <QPainter>
#include <QStyle>

TrackTableModel::TrackTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int TrackTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tracks.size();
}

int TrackTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant TrackTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size()) {
        return {};
    }

    const Track &track = m_tracks.at(index.row());

    if (role == Qt::DecorationRole && index.column() == CoverColumn) {
        const QPixmap cover = coverForTrack(track);
        return QIcon(cover);
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case CoverColumn:
            return {};
        case NameColumn:
            return displayNameForTrack(track);
        case ArtistColumn:
            return track.artist;
        case AlbumColumn:
            return track.album;
        case YearColumn:
            return track.year;
        case PathColumn:
            return track.filePath;
        default:
            return {};
        }
    }

    if (role == Qt::TextAlignmentRole) {
        if (index.column() == YearColumn) {
            return Qt::AlignCenter;
        }
    }

    return {};
}

QVariant TrackTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case CoverColumn:
        return tr("Cover");
    case NameColumn:
        return tr("Name");
    case ArtistColumn:
        return tr("Artist");
    case AlbumColumn:
        return tr("Album");
    case YearColumn:
        return tr("Year");
    case PathColumn:
        return tr("File");
    default:
        return {};
    }
}

Qt::ItemFlags TrackTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void TrackTableModel::setTracks(QVector<Track> tracks)
{
    beginResetModel();
    m_tracks = std::move(tracks);
    endResetModel();
}

void TrackTableModel::addTrack(const Track &track)
{
    const int row = m_tracks.size();
    beginInsertRows(QModelIndex(), row, row);
    m_tracks.push_back(track);
    endInsertRows();
}

Track TrackTableModel::trackAt(int row) const
{
    if (row < 0 || row >= m_tracks.size()) {
        return {};
    }
    return m_tracks.at(row);
}

void TrackTableModel::updateTrack(int row, const Track &track)
{
    if (row < 0 || row >= m_tracks.size()) {
        return;
    }

    m_tracks[row] = track;
    const QModelIndex topLeft = index(row, 0);
    const QModelIndex bottomRight = index(row, ColumnCount - 1);
    emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole, Qt::DecorationRole});
}

void TrackTableModel::removeTrack(int row)
{
    if (row < 0 || row >= m_tracks.size()) return;
    beginRemoveRows(QModelIndex(), row, row);
    m_tracks.removeAt(row);
    endRemoveRows();
}

QPixmap TrackTableModel::coverForTrack(const Track &track) const
{
    if (!track.coverArt.isNull()) {
        return track.coverArt.scaled(48, 48, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    // No cover available — return an empty pixmap so the UI can show nothing instead of a placeholder.
    return QPixmap();
}

QString TrackTableModel::displayNameForTrack(const Track &track) const
{
    if (!track.title.isEmpty()) {
        return track.title;
    }
    return QFileInfo(track.filePath).baseName();
}



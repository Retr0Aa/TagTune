#include "MetadataService.h"

#include <QBuffer>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef HAVE_TAGLIB
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/textidentificationframe.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/xiphcomment.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#endif

namespace
{
QString fallbackTextForFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    return info.baseName().isEmpty() ? QStringLiteral("Unknown Track") : info.baseName();
}
}

QVector<Track> MetadataService::loadTracks(const QStringList &filePaths) const
{
    QVector<Track> tracks;
    tracks.reserve(filePaths.size());

    for (const QString &filePath : filePaths) {
        tracks.push_back(loadTrack(filePath));
    }

    return tracks;
}

Track MetadataService::loadTrack(const QString &filePath) const
{
    Track track;
    track.filePath = filePath;
    track.title = fallbackTextForFile(filePath);
    track.artist = QStringLiteral("Unknown Artist");
    track.album = QStringLiteral("Unknown Album");
    track.albumArtist = QStringLiteral("Unknown Artist");
    track.genre = QStringLiteral("Unknown");
    track.year = QString::number(QDate::currentDate().year());

#ifdef HAVE_TAGLIB
    // Try to read from file tags using TagLib
    TagLib::FileRef f(filePath.toUtf8().constData());
    if (!f.isNull() && f.tag()) {
        TagLib::Tag *t = f.tag();
        track.title = QString::fromUtf8(t->title().toCString(true));
        track.artist = QString::fromUtf8(t->artist().toCString(true));
        track.album = QString::fromUtf8(t->album().toCString(true));
        track.genre = QString::fromUtf8(t->genre().toCString(true));
        track.comment = QString::fromUtf8(t->comment().toCString(true));
        // year and track are numeric in TagLib
        if (t->year() > 0) track.year = QString::number(t->year());
        if (t->track() > 0) track.trackNumber = QString::number(t->track());
    }

    // Try to read embedded artwork for common formats
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == QLatin1String("mp3")) {
        TagLib::MPEG::File m(filePath.toUtf8().constData());
        TagLib::ID3v2::Tag *id3v2 = m.ID3v2Tag();
        if (id3v2) {
            TagLib::ID3v2::FrameList frames = id3v2->frameList("APIC");
            if (!frames.isEmpty()) {
                auto *apic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
                if (apic) {
                    TagLib::ByteVector data = apic->picture();
                    const QByteArray bytes(data.data(), data.size());
                    QPixmap pix;
                    pix.loadFromData(reinterpret_cast<const uchar*>(bytes.constData()), bytes.size());
                    if (!pix.isNull()) track.coverArt = pix;
                }
            }
        }
        // Try to read album artist and composer from ID3v2 frames
        if (id3v2) {
            auto frames = id3v2->frameList();
            if (id3v2->frameList("TPE2").isEmpty() == false) {
                auto f = id3v2->frameList("TPE2").front();
                TagLib::ID3v2::TextIdentificationFrame *tf = dynamic_cast<TagLib::ID3v2::TextIdentificationFrame*>(f);
                if (tf) track.albumArtist = QString::fromUtf8(tf->toString().toCString(true));
            }
            if (id3v2->frameList("TCOM").isEmpty() == false) {
                auto f = id3v2->frameList("TCOM").front();
                TagLib::ID3v2::TextIdentificationFrame *tf = dynamic_cast<TagLib::ID3v2::TextIdentificationFrame*>(f);
                if (tf) track.composer = QString::fromUtf8(tf->toString().toCString(true));
            }
            if (id3v2->frameList("TPOS").isEmpty() == false) {
                auto f = id3v2->frameList("TPOS").front();
                TagLib::ID3v2::TextIdentificationFrame *tf = dynamic_cast<TagLib::ID3v2::TextIdentificationFrame*>(f);
                if (tf) track.discNumber = QString::fromUtf8(tf->toString().toCString(true));
            }
        }
    } else if (ext == QLatin1String("flac")) {
        TagLib::FLAC::File ff(filePath.toUtf8().constData());
        auto pics = ff.pictureList();
        if (!pics.isEmpty()) {
            TagLib::FLAC::Picture *pic = pics.front();
            if (pic) {
                TagLib::ByteVector data = pic->data();
                const QByteArray bytes(data.data(), data.size());
                QPixmap pix;
                pix.loadFromData(reinterpret_cast<const uchar*>(bytes.constData()), bytes.size());
                if (!pix.isNull()) track.coverArt = pix;
            }
        }
        // Read Vorbis/FLAC comment fields if present
        if (ff.xiphComment()) {
            TagLib::Ogg::XiphComment *xc = ff.xiphComment();
            const auto &map = xc->fieldListMap();
            if (map.contains("ALBUMARTIST")) {
                const auto &list = map["ALBUMARTIST"];
                if (!list.isEmpty()) track.albumArtist = QString::fromUtf8(list.front().toCString(true));
            }
            if (map.contains("COMPOSER")) {
                const auto &list = map["COMPOSER"];
                if (!list.isEmpty()) track.composer = QString::fromUtf8(list.front().toCString(true));
            }
            if (map.contains("DISCNUMBER")) {
                const auto &list = map["DISCNUMBER"];
                if (!list.isEmpty()) track.discNumber = QString::fromUtf8(list.front().toCString(true));
            }
        }
    } else if (ext == QLatin1String("m4a") || ext == QLatin1String("mp4")) {
        TagLib::MP4::File mp4(filePath.toUtf8().constData());
        TagLib::MP4::Tag *tag = mp4.tag();
        if (tag) {
            // cover art
            {
                TagLib::MP4::Item item = tag->item("covr");
                if (item.isValid()) {
                    auto coverList = item.toCoverArtList();
                    if (!coverList.isEmpty()) {
                        TagLib::MP4::CoverArt art = coverList.front();
                        TagLib::ByteVector data = art.data();
                        const QByteArray bytes(data.data(), data.size());
                        QPixmap pix;
                        pix.loadFromData(reinterpret_cast<const uchar*>(bytes.constData()), bytes.size());
                        if (!pix.isNull()) track.coverArt = pix;
                    }
                }
            }

            // aART (album artist)
            {
                TagLib::MP4::Item item = tag->item("aART");
                if (item.isValid()) {
                    auto sl = item.toStringList();
                    if (!sl.isEmpty()) track.albumArtist = QString::fromUtf8(sl.front().toCString(true));
                }
            }

            // ©wrt (composer)
            {
                TagLib::MP4::Item item = tag->item("©wrt");
                if (item.isValid()) {
                    auto sl = item.toStringList();
                    if (!sl.isEmpty()) track.composer = QString::fromUtf8(sl.front().toCString(true));
                }
            }

            // disk
            {
                TagLib::MP4::Item item = tag->item("disk");
                if (item.isValid()) {
                    auto p = item.toIntPair();
                    if (p.first > 0) track.discNumber = QString::number(p.first);
                }
            }
        }
    }

    return applyDefaults(track);
#else
    // TagLib is required for in-file tagging in this build.
    return applyDefaults(track);
#endif
}

bool MetadataService::saveTrack(const Track &track, QString *errorMessage) const
{
#ifdef HAVE_TAGLIB
    // Try to write tags into the file using TagLib. If anything fails, fall back to sidecar.
    const QString filePath = track.filePath;
    const QString ext = QFileInfo(filePath).suffix().toLower();

    try {
        if (ext == QLatin1String("mp3")) {
            TagLib::MPEG::File m(filePath.toUtf8().constData());
            TagLib::ID3v2::Tag *id3v2 = m.ID3v2Tag(true);
            TagLib::Tag *t = m.tag();
            if (t) {
                t->setTitle(track.title.toUtf8().constData());
                t->setArtist(track.artist.toUtf8().constData());
                t->setAlbum(track.album.toUtf8().constData());
                t->setGenre(track.genre.toUtf8().constData());
                t->setComment(track.comment.toUtf8().constData());
                if (!track.year.isEmpty()) t->setYear(track.year.toInt());
                if (!track.trackNumber.isEmpty()) t->setTrack(track.trackNumber.toInt());
                        // album artist and composer are not part of TagLib::Tag base — set via ID3v2 specific frames
                        // for MP3 we'll set below using ID3v2 frames
            }

            // Artwork
            if (!track.coverArt.isNull() && id3v2) {
                // remove existing APIC frames
                id3v2->removeFrames("APIC");
                QByteArray bytes;
                QBuffer buf(&bytes);
                buf.open(QIODevice::WriteOnly);
                track.coverArt.save(&buf, "PNG");
                TagLib::ID3v2::AttachedPictureFrame *apic = new TagLib::ID3v2::AttachedPictureFrame;
                apic->setMimeType("image/png");
                apic->setPicture(TagLib::ByteVector(bytes.constData(), bytes.size()));
                id3v2->addFrame(apic);
            }

            // Set extended frames: TPE2 (album artist), TCOM (composer), TPOS (disc)
            if (id3v2) {
                // Helper to create/replace a text frame
                auto setTextFrame = [id3v2](const char *frameId, const QString &value) {
                    if (value.isEmpty()) return;
                    // Remove existing frames
                    id3v2->removeFrames(frameId);
                    TagLib::ID3v2::TextIdentificationFrame *tf = new TagLib::ID3v2::TextIdentificationFrame(frameId, TagLib::String::UTF8);
                    tf->setText(TagLib::String(value.toUtf8().constData(), TagLib::String::UTF8));
                    id3v2->addFrame(tf);
                };

                setTextFrame("TPE2", track.albumArtist);
                setTextFrame("TCOM", track.composer);
                // TPOS expects disc like "1/2" or "1"
                if (!track.discNumber.isEmpty()) {
                    setTextFrame("TPOS", track.discNumber);
                }
            }

            m.save();
            return true;
        } else if (ext == QLatin1String("flac")) {
            TagLib::FLAC::File f(filePath.toUtf8().constData());
            TagLib::Tag *t = f.tag();
            if (t) {
                t->setTitle(track.title.toUtf8().constData());
                t->setArtist(track.artist.toUtf8().constData());
                t->setAlbum(track.album.toUtf8().constData());
                t->setGenre(track.genre.toUtf8().constData());
                t->setComment(track.comment.toUtf8().constData());
                if (!track.year.isEmpty()) t->setYear(track.year.toInt());
                if (!track.trackNumber.isEmpty()) t->setTrack(track.trackNumber.toInt());
            }

            if (!track.coverArt.isNull()) {
                QByteArray bytes;
                QBuffer buf(&bytes);
                buf.open(QIODevice::WriteOnly);
                track.coverArt.save(&buf, "PNG");
                TagLib::FLAC::Picture *pic = new TagLib::FLAC::Picture;
                pic->setMimeType("image/png");
                pic->setData(TagLib::ByteVector(bytes.constData(), bytes.size()));
                f.addPicture(pic);
            }

            f.save();
            return true;
        } else if (ext == QLatin1String("m4a") || ext == QLatin1String("mp4")) {
            TagLib::MP4::File f(filePath.toUtf8().constData());
            TagLib::MP4::Tag *t = f.tag();
            if (t) {
                t->setTitle(track.title.toUtf8().constData());
                t->setArtist(track.artist.toUtf8().constData());
                t->setAlbum(track.album.toUtf8().constData());
                // set album artist as 'aART'
                if (!track.albumArtist.isEmpty()) {
                    TagLib::StringList sl;
                    sl.append(TagLib::String(track.albumArtist.toUtf8().constData(), TagLib::String::UTF8));
                    TagLib::MP4::Item item(sl);
                    t->setItem("aART", item);
                }
                if (!track.composer.isEmpty()) {
                    TagLib::StringList sl;
                    sl.append(TagLib::String(track.composer.toUtf8().constData(), TagLib::String::UTF8));
                    TagLib::MP4::Item item(sl);
                    t->setItem("©wrt", item);
                }
                if (!track.discNumber.isEmpty()) {
                    // disk item is usually an int pair (number/total) or single int
                    int d = track.discNumber.toInt();
                    TagLib::MP4::Item item(d);
                    t->setItem("disk", item);
                }
            }

            if (!track.coverArt.isNull()) {
                QByteArray bytes;
                QBuffer buf(&bytes);
                buf.open(QIODevice::WriteOnly);
                track.coverArt.save(&buf, "PNG");
                TagLib::ByteVector bv(bytes.constData(), bytes.size());
                TagLib::MP4::CoverArt art(TagLib::MP4::CoverArt::PNG, bv);
                TagLib::MP4::CoverArtList list;
                list.append(art);
                TagLib::MP4::Item item(list);
                t->setItem("covr", item);
            }

            f.save();
            return true;
        }
    } catch (const std::exception &e) {
        if (errorMessage) *errorMessage = QString::fromUtf8(e.what());
        return false;
    } catch (...) {
        if (errorMessage) *errorMessage = QStringLiteral("Unknown error while writing tags");
        return false;
    }
#else
    Q_UNUSED(track);
    Q_UNUSED(errorMessage);
    return false; // TagLib not available
#endif
}

Track MetadataService::applyDefaults(const Track &track)
{
    Track normalized = track;

    if (normalized.title.trimmed().isEmpty()) {
        normalized.title = fallbackTextForFile(track.filePath);
    }
    if (normalized.artist.trimmed().isEmpty()) {
        normalized.artist = QStringLiteral("Unknown Artist");
    }
    if (normalized.album.trimmed().isEmpty()) {
        normalized.album = QStringLiteral("Unknown Album");
    }
    if (normalized.albumArtist.trimmed().isEmpty()) {
        normalized.albumArtist = QStringLiteral("Unknown Artist");
    }
    if (normalized.genre.trimmed().isEmpty()) {
        normalized.genre = QStringLiteral("Unknown");
    }
    if (normalized.year.trimmed().isEmpty()) {
        normalized.year = QString::number(QDate::currentDate().year());
    }

    return normalized;
}





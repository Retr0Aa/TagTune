#include "MetadataService.h"

#include <QBuffer>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QDebug>

#ifdef HAVE_TAGLIB
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/wavfile.h>
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

QString sidecarPathForFile(const QString &filePath)
{
    return filePath + QLatin1String(".tagtune.json");
}

QString sidecarCoverPathForFile(const QString &filePath)
{
    // store cover next to the audio file with a predictable suffix
    return filePath + QLatin1String(".tagtune.cover.png");
}

QPixmap pixmapFromTagLibBytes(const TagLib::ByteVector &data, const char *mimeType = nullptr)
{
    const QByteArray bytes(data.data(), data.size());
    QPixmap pix;
    bool ok = pix.loadFromData(reinterpret_cast<const uchar *>(bytes.constData()), bytes.size());
    if (!ok || pix.isNull()) {
        QImage img;
        if (mimeType) {
            ok = img.loadFromData(bytes, mimeType);
        } else {
            ok = img.loadFromData(bytes);
        }
        if (ok && !img.isNull()) {
            pix = QPixmap::fromImage(img);
        }
    }
    return pix;
}

bool loadEmbeddedArtworkFromId3v2(TagLib::ID3v2::Tag *id3v2, Track &track, const QString &sourceLabel)
{
    if (!id3v2) return false;

    TagLib::ID3v2::FrameList frames = id3v2->frameList("APIC");
    if (frames.isEmpty()) {
        frames = id3v2->frameList("PIC");
    }

    qDebug() << sourceLabel << "APIC/PIC frames=" << frames.size();
    for (auto fr : frames) {
        auto *apic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(fr);
        if (!apic) {
            continue;
        }

        const QString mime = QString::fromUtf8(apic->mimeType().toCString(true));
        const TagLib::ByteVector data = apic->picture();
        const QByteArray bytes(data.data(), data.size());

        const char *fmt = nullptr;
        if (mime.contains("jpeg", Qt::CaseInsensitive) || mime.contains("jpg", Qt::CaseInsensitive)) {
            fmt = "JPG";
        } else if (mime.contains("png", Qt::CaseInsensitive)) {
            fmt = "PNG";
        } else if (mime.contains("gif", Qt::CaseInsensitive)) {
            fmt = "GIF";
        }

        QPixmap pix = pixmapFromTagLibBytes(data, fmt);
        if (!pix.isNull()) {
            track.coverArt = pix;
            qDebug() << sourceLabel << "loaded cover mime=" << mime << "bytes=" << bytes.size();
            return true;
        }
    }

    return false;
}

bool saveSidecar(const Track &track)
{
    const QString path = sidecarPathForFile(track.filePath);
    QJsonObject obj;
    obj["title"] = track.title;
    obj["artist"] = track.artist;
    obj["album"] = track.album;
    obj["albumArtist"] = track.albumArtist;
    obj["genre"] = track.genre;
    obj["comment"] = track.comment;
    obj["composer"] = track.composer;
    obj["year"] = track.year;
    obj["trackNumber"] = track.trackNumber;
    obj["discNumber"] = track.discNumber;
    obj["bpm"] = track.bpm;

    // Save cover as separate PNG next to the audio file
    if (!track.coverArt.isNull()) {
        const QString coverPath = sidecarCoverPathForFile(track.filePath);
        if (!track.coverArt.save(coverPath, "PNG")) {
            qDebug() << "saveSidecar: failed to save cover" << coverPath;
        } else {
            obj["cover"] = QFileInfo(coverPath).fileName();
        }
    }

    QJsonDocument doc(obj);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qDebug() << "saveSidecar: failed to open" << path;
        return false;
    }
    f.write(doc.toJson());
    f.close();
    qDebug() << "saveSidecar: wrote" << path;
    return true;
}

bool loadSidecar(const QString &filePath, Track &out)
{
    const QString path = sidecarPathForFile(filePath);
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;
    const QJsonObject obj = doc.object();
    if (obj.contains("title")) out.title = obj.value("title").toString();
    if (obj.contains("artist")) out.artist = obj.value("artist").toString();
    if (obj.contains("album")) out.album = obj.value("album").toString();
    if (obj.contains("albumArtist")) out.albumArtist = obj.value("albumArtist").toString();
    if (obj.contains("genre")) out.genre = obj.value("genre").toString();
    if (obj.contains("comment")) out.comment = obj.value("comment").toString();
    if (obj.contains("composer")) out.composer = obj.value("composer").toString();
    if (obj.contains("year")) out.year = obj.value("year").toString();
    if (obj.contains("trackNumber")) out.trackNumber = obj.value("trackNumber").toString();
    if (obj.contains("discNumber")) out.discNumber = obj.value("discNumber").toString();
    if (obj.contains("bpm")) out.bpm = obj.value("bpm").toString();

    if (obj.contains("cover")) {
        // cover file is stored next to audio file
        const QString coverFile = QFileInfo(filePath).absolutePath() + QLatin1Char('/') + obj.value("cover").toString();
        QPixmap pix;
        if (pix.load(coverFile)) out.coverArt = pix;
    } else {
        // try the predictable cover path
        const QString coverFile = sidecarCoverPathForFile(filePath);
        QPixmap pix;
        if (pix.load(coverFile)) out.coverArt = pix;
    }

    return true;
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
    TagLib::FileRef fref(filePath.toUtf8().constData());
    if (!fref.isNull() && fref.tag()) {
        TagLib::Tag *t = fref.tag();
        track.title = QString::fromUtf8(t->title().toCString(true));
        track.artist = QString::fromUtf8(t->artist().toCString(true));
        track.album = QString::fromUtf8(t->album().toCString(true));
        track.genre = QString::fromUtf8(t->genre().toCString(true));
        track.comment = QString::fromUtf8(t->comment().toCString(true));
        // year and track are numeric in TagLib
        if (t->year() > 0) track.year = QString::number(t->year());
        if (t->track() > 0) track.trackNumber = QString::number(t->track());
    }

    // Use the already opened FileRef's underlying file object for format-specific access
    TagLib::File *underlying = fref.file();

    // Try to read embedded artwork for common formats
    const QString ext = QFileInfo(filePath).suffix().toLower();
    qDebug() << "loadTrack: loading" << filePath << "ext=" << ext;
    if (ext == QLatin1String("mp3")) {
        TagLib::MPEG::File *m = dynamic_cast<TagLib::MPEG::File*>(underlying);
        qDebug() << "loadTrack: MPEG::File via FileRef available=" << (m != nullptr);
        if (m) {
            TagLib::ID3v2::Tag *id3v2 = m->ID3v2Tag();
            qDebug() << "loadTrack: id3v2 tag present=" << (id3v2 != nullptr);
            if (id3v2) {
                // list some frame ids for diagnostics
                TagLib::ID3v2::FrameList allFrames = id3v2->frameList();
                qDebug() << "loadTrack: total frames=" << allFrames.size();
                int logCount = 0;
                for (auto fr : allFrames) {
                    if (logCount++ >= 20) break;
                    const TagLib::ByteVector frameId = fr->frameID();
                    qDebug() << "  frame:" << QString::fromUtf8(frameId.data(), frameId.size());
                }

                loadEmbeddedArtworkFromId3v2(id3v2, track, QStringLiteral("loadTrack: mp3"));
            }
            // Try to read album artist and composer from ID3v2 frames
            if (id3v2) {
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
        }
    } else if (ext == QLatin1String("wav") || ext == QLatin1String("wave")) {
        TagLib::RIFF::WAV::File *wf = dynamic_cast<TagLib::RIFF::WAV::File *>(underlying);
        qDebug() << "loadTrack: WAV::File via FileRef available=" << (wf != nullptr);
        if (wf) {
            TagLib::ID3v2::Tag *id3v2 = wf->ID3v2Tag();
            qDebug() << "loadTrack: wav id3v2 tag present=" << (id3v2 != nullptr);
            if (id3v2) {
                loadEmbeddedArtworkFromId3v2(id3v2, track, QStringLiteral("loadTrack: wav"));

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
        }
    } else if (ext == QLatin1String("flac")) {
        TagLib::FLAC::File *ff = dynamic_cast<TagLib::FLAC::File*>(underlying);
        qDebug() << "loadTrack: FLAC::File via FileRef available=" << (ff != nullptr);
        if (ff) {
            auto pics = ff->pictureList();
            qDebug() << "loadTrack: flac pictures=" << pics.size();
            if (!pics.isEmpty()) {
                TagLib::FLAC::Picture *pic = pics.front();
                if (pic) {
                    TagLib::ByteVector data = pic->data();
                    const QByteArray bytes(data.data(), data.size());
                    QPixmap pix;
                    bool ok = pix.loadFromData(reinterpret_cast<const uchar*>(bytes.constData()), bytes.size());
                    if (!ok || pix.isNull()) {
                        QImage img;
                        img.loadFromData(bytes);
                        if (!img.isNull()) pix = QPixmap::fromImage(img);
                    }
                    if (!pix.isNull()) track.coverArt = pix;
                }
            }
            // Read Vorbis/FLAC comment fields if present
            if (ff->xiphComment()) {
                TagLib::Ogg::XiphComment *xc = ff->xiphComment();
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
        }
    } else if (ext == QLatin1String("m4a") || ext == QLatin1String("mp4")) {
        TagLib::MP4::File *mp4 = dynamic_cast<TagLib::MP4::File*>(underlying);
        qDebug() << "loadTrack: MP4::File via FileRef available=" << (mp4 != nullptr);
        if (mp4) {
            TagLib::MP4::Tag *tag = mp4->tag();
            qDebug() << "loadTrack: mp4 tag present=" << (tag != nullptr);
            if (tag) {
                // cover art
                TagLib::MP4::Item item = tag->item("covr");
                if (item.isValid()) {
                    auto coverList = item.toCoverArtList();
                    qDebug() << "loadTrack: mp4 coverList size=" << coverList.size();
                    if (!coverList.isEmpty()) {
                        TagLib::MP4::CoverArt art = coverList.front();
                        TagLib::ByteVector data = art.data();
                        const QByteArray bytes(data.data(), data.size());
                        QPixmap pix;
                        bool ok = pix.loadFromData(reinterpret_cast<const uchar*>(bytes.constData()), bytes.size());
                        if (!ok || pix.isNull()) {
                            QImage img;
                            img.loadFromData(bytes);
                            if (!img.isNull()) pix = QPixmap::fromImage(img);
                        }
                        if (!pix.isNull()) track.coverArt = pix;
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
    }

    // If TagLib didn't provide tags/cover, try loading a sidecar saved by this app.
    Track sidecarCandidate = track;
    if (loadSidecar(filePath, sidecarCandidate)) {
        // Merge non-empty fields from sidecar
        if (!sidecarCandidate.title.trimmed().isEmpty()) track.title = sidecarCandidate.title;
        if (!sidecarCandidate.artist.trimmed().isEmpty()) track.artist = sidecarCandidate.artist;
        if (!sidecarCandidate.album.trimmed().isEmpty()) track.album = sidecarCandidate.album;
        if (!sidecarCandidate.albumArtist.trimmed().isEmpty()) track.albumArtist = sidecarCandidate.albumArtist;
        if (!sidecarCandidate.genre.trimmed().isEmpty()) track.genre = sidecarCandidate.genre;
        if (!sidecarCandidate.comment.trimmed().isEmpty()) track.comment = sidecarCandidate.comment;
        if (!sidecarCandidate.composer.trimmed().isEmpty()) track.composer = sidecarCandidate.composer;
        if (!sidecarCandidate.year.trimmed().isEmpty()) track.year = sidecarCandidate.year;
        if (!sidecarCandidate.trackNumber.trimmed().isEmpty()) track.trackNumber = sidecarCandidate.trackNumber;
        if (!sidecarCandidate.discNumber.trimmed().isEmpty()) track.discNumber = sidecarCandidate.discNumber;
        if (!sidecarCandidate.bpm.trimmed().isEmpty()) track.bpm = sidecarCandidate.bpm;
        if (!sidecarCandidate.coverArt.isNull()) track.coverArt = sidecarCandidate.coverArt;
    }

    qDebug() << "loadTrack: final cover isNull=" << track.coverArt.isNull();
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
            }

            // Artwork
            if (!track.coverArt.isNull() && id3v2) {
                // remove existing APIC and PIC frames
                id3v2->removeFrames("APIC");
                id3v2->removeFrames("PIC");
                QByteArray bytes;
                QBuffer buf(&bytes);
                buf.open(QIODevice::WriteOnly);
                track.coverArt.save(&buf, "PNG");
                TagLib::ID3v2::AttachedPictureFrame *apic = new TagLib::ID3v2::AttachedPictureFrame;
                apic->setMimeType("image/png");
                apic->setPicture(TagLib::ByteVector(bytes.constData(), bytes.size()));
                id3v2->addFrame(apic);
            }

            if (m.save()) return true;
            // fallback to sidecar
            if (saveSidecar(track)) return true;
            if (errorMessage) *errorMessage = QStringLiteral("Unable to write MP3 tags or sidecar");
            return false;
        } else if (ext == QLatin1String("wav") || ext == QLatin1String("wave")) {
            TagLib::RIFF::WAV::File f(filePath.toUtf8().constData());
            TagLib::ID3v2::Tag *id3v2 = f.ID3v2Tag();
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

            if (!track.coverArt.isNull() && id3v2) {
                id3v2->removeFrames("APIC");
                id3v2->removeFrames("PIC");
                QByteArray bytes;
                QBuffer buf(&bytes);
                buf.open(QIODevice::WriteOnly);
                track.coverArt.save(&buf, "PNG");
                TagLib::ID3v2::AttachedPictureFrame *apic = new TagLib::ID3v2::AttachedPictureFrame;
                apic->setMimeType("image/png");
                apic->setPicture(TagLib::ByteVector(bytes.constData(), bytes.size()));
                id3v2->addFrame(apic);
            }

            if (f.save()) return true;
            if (saveSidecar(track)) return true;
            if (errorMessage) *errorMessage = QStringLiteral("Unable to write WAV tags or sidecar");
            return false;
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

            if (f.save()) return true;
            if (saveSidecar(track)) return true;
            if (errorMessage) *errorMessage = QStringLiteral("Unable to write FLAC tags or sidecar");
            return false;
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

            if (f.save()) return true;
            if (saveSidecar(track)) return true;
            if (errorMessage) *errorMessage = QStringLiteral("Unable to write MP4 tags or sidecar");
            return false;
        } else {
            // Generic fallback: try using TagLib::FileRef to write common tag fields for other formats
            TagLib::FileRef ref(filePath.toUtf8().constData());
            TagLib::File *genericFile = ref.file();
            if (genericFile && genericFile->tag()) {
                TagLib::Tag *t = genericFile->tag();
                t->setTitle(track.title.toUtf8().constData());
                t->setArtist(track.artist.toUtf8().constData());
                t->setAlbum(track.album.toUtf8().constData());
                t->setGenre(track.genre.toUtf8().constData());
                t->setComment(track.comment.toUtf8().constData());
                if (!track.year.isEmpty()) t->setYear(track.year.toInt());
                if (!track.trackNumber.isEmpty()) t->setTrack(track.trackNumber.toInt());
                // Attempt to save via the underlying file object
                if (genericFile->save()) return true;
                // If save failed, fall through to sidecar
            }

            if (saveSidecar(track)) return true;
            if (errorMessage) *errorMessage = QStringLiteral("Unsupported format or unable to write tags for this file type");
            return false;
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


















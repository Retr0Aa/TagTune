package org.retr0a.tagtune;

import org.jaudiotagger.audio.AudioFile;
import org.jaudiotagger.audio.AudioFileIO;
import org.jaudiotagger.tag.FieldKey;
import org.jaudiotagger.tag.Tag;
import org.jaudiotagger.tag.TagField;
import org.jaudiotagger.tag.images.Artwork;
import org.jaudiotagger.tag.images.ArtworkFactory;

import javax.imageio.ImageIO;
import javax.swing.*;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class AudioFileProcessor {
    public static final int THUMBNAIL_SIZE = 32;
    public static final int LARGE_COVER_SIZE = 150;

    public static boolean isAudioFile(File file) {
        String name = file.getName().toLowerCase();
        return name.endsWith(".mp3") || name.endsWith(".m4a") || 
               name.endsWith(".flac") || name.endsWith(".wav") || 
               name.endsWith(".ogg");
    }

    public static List<File> scanDirectory(File dir) {
        List<File> result = new ArrayList<>();
        File[] files = dir.listFiles();
        if (files != null) {
            for (File f : files) {
                if (f.isDirectory()) result.addAll(scanDirectory(f));
                else if (isAudioFile(f)) result.add(f);
            }
        }
        return result;
    }

    public static AudioMetadata extractMetadata(File file) {
        String title = "", artist = "", album = "", year = "", track = "", genre = "", comment = "", composer = "", disc = "";
        ImageIcon icon = null;
        ImageIcon largeIcon = null;
        try {
            AudioFile f = AudioFileIO.read(file);
            Tag tag = f.getTag();
            if (tag != null) {
                title = tag.getFirst(FieldKey.TITLE);
                artist = tag.getFirst(FieldKey.ARTIST);
                album = tag.getFirst(FieldKey.ALBUM);
                year = cleanYear(tag.getFirst(FieldKey.YEAR));
                track = tag.getFirst(FieldKey.TRACK);
                genre = tag.getFirst(FieldKey.GENRE);
                comment = tag.getFirst(FieldKey.COMMENT);
                composer = tag.getFirst(FieldKey.COMPOSER);
                disc = tag.getFirst(FieldKey.DISC_NO);
                
                Artwork art = tag.getFirstArtwork();
                if (art != null) {
                    BufferedImage img = ImageIO.read(new ByteArrayInputStream(art.getBinaryData()));
                    if (img != null) {
                        icon = new ImageIcon(ImageUtils.scaleImage(img, THUMBNAIL_SIZE, THUMBNAIL_SIZE));
                        largeIcon = new ImageIcon(ImageUtils.scaleImage(img, LARGE_COVER_SIZE, LARGE_COVER_SIZE));
                    }
                }
            }
        } catch (Exception ignored) {}
        return new AudioMetadata(file.getName(), file.getAbsolutePath(), title, artist, album, year, track, genre, comment, composer, disc, icon, largeIcon);
    }

    private static String cleanYear(String year) {
        if (year == null || year.isEmpty()) return "";
        // If it's a date like 2024-01-01, just take 2024
        if (year.length() >= 4) {
            String first4 = year.substring(0, 4);
            if (first4.matches("\\d{4}")) return first4;
        }
        return year;
    }

    public static boolean updateMetadata(String filePath, String title, String artist, String album, String year, String track, String genre, String comment, String composer, String disc, File newArtwork) {
        try {
            File file = new File(filePath);
            AudioFile f = AudioFileIO.read(file);
            Tag tag = f.getTagOrCreateAndSetDefault();

            setSafeField(tag, FieldKey.TITLE, title);
            setSafeField(tag, FieldKey.ARTIST, artist);
            setSafeField(tag, FieldKey.ALBUM, album);
            setSafeField(tag, FieldKey.YEAR, year);
            setSafeField(tag, FieldKey.TRACK, track);
            setSafeField(tag, FieldKey.GENRE, genre);
            setSafeField(tag, FieldKey.COMMENT, comment);
            setSafeField(tag, FieldKey.COMPOSER, composer);
            setSafeField(tag, FieldKey.DISC_NO, disc);
            
            if (newArtwork != null && newArtwork.exists()) {
                try {
                    tag.deleteArtworkField();
                    Artwork art = ArtworkFactory.createArtworkFromFile(newArtwork);
                    tag.setField(art);
                } catch (Exception e) {
                    System.err.println("Artwork update failed for " + filePath + ": " + e.getMessage());
                }
            }
            
            f.commit();
            return true;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    private static void setSafeField(Tag tag, FieldKey key, String value) {
        if (value == null) return;
        try {
            tag.setField(key, value);
        } catch (Exception e) {
            try {
                TagField field = tag.createField(key, value);
                if (field != null) tag.setField(field);
            } catch (Exception ignored) {}
        }
    }

    public static boolean updateMetadata(String filePath, String title, String artist, String album, String year, String track, String genre, String comment, String composer, String disc) {
        return updateMetadata(filePath, title, artist, album, year, track, genre, comment, composer, disc, null);
    }
}

package org.retr0a.tagtune;

import javax.swing.*;

public class AudioMetadata {
    public String title;
    public String artistName;
    public String album;
    public String year;
    public String trackNumber;
    public String genre;
    public String comment;
    public String composer;
    public String discNumber;
    public String fileName;
    public String filePath;
    public ImageIcon coverArt;
    public ImageIcon largeCoverArt;

    public AudioMetadata(String fileName, String filePath, String title, String artistName, String album, String year, String trackNumber, String genre, String comment, String composer, String discNumber, ImageIcon coverArt, ImageIcon largeCoverArt) {
        this.fileName = fileName;
        this.filePath = filePath;
        this.title = title;
        this.artistName = artistName;
        this.album = album;
        this.year = year;
        this.trackNumber = trackNumber;
        this.genre = genre;
        this.comment = comment;
        this.composer = composer;
        this.discNumber = discNumber;
        this.coverArt = coverArt;
        this.largeCoverArt = largeCoverArt;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof AudioMetadata) {
            return this.filePath.equals(((AudioMetadata) obj).filePath);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return filePath.hashCode();
    }
}

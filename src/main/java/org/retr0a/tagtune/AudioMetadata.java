package org.retr0a.tagtune;

import javax.swing.*;

public class AudioMetadata {
    public String title;
    public String artistName;
    public String releaseDate;
    public String fileName;
    public String filePath;
    public ImageIcon coverArt;
    public ImageIcon largeCoverArt;

    public AudioMetadata(String fileName, String filePath, String title, String artistName, String releaseDate, ImageIcon coverArt, ImageIcon largeCoverArt) {
        this.fileName = fileName;
        this.filePath = filePath;
        this.title = title;
        this.artistName = artistName;
        this.releaseDate = releaseDate;
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

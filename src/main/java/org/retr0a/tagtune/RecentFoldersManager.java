package org.retr0a.tagtune;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.prefs.Preferences;

public class RecentFoldersManager {
    private static final String PREF_RECENT_FOLDERS = "recent_folders";
    private static final int MAX_RECENT = 5;
    private final Preferences prefs;

    public RecentFoldersManager() {
        this.prefs = Preferences.userNodeForPackage(RecentFoldersManager.class);
    }

    public List<String> getRecentFolders() {
        String recentStr = prefs.get(PREF_RECENT_FOLDERS, "");
        if (recentStr.isEmpty()) return new ArrayList<>();
        return new ArrayList<>(Arrays.asList(recentStr.split("\\|")));
    }

    public void addToRecent(String path) {
        List<String> paths = getRecentFolders();
        paths.remove(path);
        paths.add(0, path);
        if (paths.size() > MAX_RECENT) {
            paths = paths.subList(0, MAX_RECENT);
        }
        prefs.put(PREF_RECENT_FOLDERS, String.join("|", paths));
    }
}

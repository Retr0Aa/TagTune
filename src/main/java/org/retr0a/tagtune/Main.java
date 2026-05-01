package org.retr0a.tagtune;

import javax.imageio.ImageIO;
import javax.swing.*;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.TableCellRenderer;
import javax.swing.text.AbstractDocument;
import javax.swing.text.AttributeSet;
import javax.swing.text.BadLocationException;
import javax.swing.text.DocumentFilter;
import java.awt.*;
import java.awt.datatransfer.DataFlavor;
import java.awt.event.InputEvent;
import java.awt.event.KeyEvent;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.image.BufferedImage;
import java.io.File;
import java.net.URL;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.List;
import java.util.function.Consumer;
import java.util.logging.Level;
import java.util.logging.Logger;

public class Main {
    private static final Set<String> loadedFilePaths = Collections.synchronizedSet(new HashSet<>());
    private static final RecentFoldersManager recentManager = new RecentFoldersManager();
    private static JMenu recentMenu;
    private static JMenuItem saveMenuItem;
    private static File newCoverFile = null;

    private static final String[] COMMON_GENRES = {
        "", "Rock", "Pop", "Jazz", "Classical", "Hip-Hop", "Electronic", "R&B", "Country", 
        "Blues", "Reggae", "Metal", "Folk", "Punk", "Soul", "Funk", "Techno", "House", "Ambient"
    };

    public static void main(String[] args) {
        // Silence noisy jaudiotagger INFO logs that appear red in console
        Logger.getLogger("org.jaudiotagger").setLevel(Level.WARNING);

        System.setProperty("apple.awt.application.name", "TagTune");
        System.setProperty("apple.laf.useScreenMenuBar", "true");
        System.setProperty("com.apple.mrj.application.apple.menu.about.name", "TagTune");
        System.setProperty("apple.awt.fileDialogForDirectories", "true");

        SwingUtilities.invokeLater(() -> {
            JFrame frame = new JFrame("TagTune");
            frame.setSize(1200, 800);
            frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            frame.setLayout(new BorderLayout());

            // Load app icon from resources and apply native macOS effects
            Image appIcon = null;
            try {
                URL iconUrl = Main.class.getResource("/Icon-iOS-Default-1024x1024@1x.png");
                if (iconUrl != null) {
                    Image rawIcon = ImageIO.read(iconUrl);
                    appIcon = ImageUtils.applyMacEffects(rawIcon);
                } else {
                    appIcon = ImageUtils.createAppIcon(512);
                }
            } catch (Exception e) {
                appIcon = ImageUtils.createAppIcon(512);
            }

            frame.setIconImage(appIcon);
            if (Taskbar.isTaskbarSupported()) {
                try { 
                    Taskbar.getTaskbar().setIconImage(appIcon); 
                } catch (Exception ignored) {}
            }

            // Table Model
            String[] columnNames = {"Cover", "File Name", "Title", "Artist", "Album", "Year", "Track", "Genre", "Disc", "Composer", "Comment", "Path"};
            DefaultTableModel model = new DefaultTableModel(columnNames, 0) {
                @Override public boolean isCellEditable(int row, int col) { return false; }
                @Override public Class<?> getColumnClass(int col) { return col == 0 ? ImageIcon.class : Object.class; }
            };

            // Table UI
            JTable table = createTable(model);
            table.getColumnModel().removeColumn(table.getColumnModel().getColumn(11)); // Hide Path

            // Edit Panel
            JPanel editPanel = new JPanel(new BorderLayout(15, 10));
            editPanel.setBorder(BorderFactory.createEmptyBorder(10, 20, 15, 20));
            
            JLabel lblEditTitle = new JLabel("No file selected - Edit");
            lblEditTitle.setFont(new Font("SansSerif", Font.BOLD, 15));
            lblEditTitle.setForeground(new Color(40, 40, 40));
            editPanel.add(lblEditTitle, BorderLayout.NORTH);

            JLabel lblCover = new JLabel();
            lblCover.setPreferredSize(new Dimension(AudioFileProcessor.LARGE_COVER_SIZE, AudioFileProcessor.LARGE_COVER_SIZE));
            lblCover.setBorder(BorderFactory.createLineBorder(Color.LIGHT_GRAY));
            lblCover.setHorizontalAlignment(SwingConstants.CENTER);
            lblCover.setVerticalTextPosition(SwingConstants.CENTER);
            lblCover.setHorizontalTextPosition(SwingConstants.CENTER);
            lblCover.setCursor(new Cursor(Cursor.HAND_CURSOR));
            lblCover.setToolTipText("Click to change cover art");

            JPanel infoPanel = new JPanel();
            infoPanel.setLayout(new BoxLayout(infoPanel, BoxLayout.Y_AXIS));
            JLabel lblFileName = new JLabel("File: ");
            lblFileName.setFont(new Font("SansSerif", Font.ITALIC, 11));
            infoPanel.add(lblCover);
            infoPanel.add(Box.createVerticalStrut(8));
            infoPanel.add(lblFileName);

            JPanel fieldsPanel = new JPanel(new GridBagLayout());
            GridBagConstraints gbc = new GridBagConstraints();
            gbc.fill = GridBagConstraints.HORIZONTAL;
            gbc.insets = new Insets(4, 10, 4, 10);
            gbc.weightx = 0;

            JTextField tfTitle = new JTextField(20);
            JTextField tfArtist = new JTextField(20);
            JTextField tfAlbum = new JTextField(20);
            JTextField tfYear = new JTextField(10);
            JTextField tfTrack = new JTextField(10);
            
            JComboBox<String> cbGenre = new JComboBox<>(COMMON_GENRES);
            cbGenre.setEditable(true);
            
            JTextField tfDisc = new JTextField(10);
            JTextField tfComposer = new JTextField(20);
            JTextField tfComment = new JTextField(20);

            // Apply numeric validation
            applyNumericFilter(tfYear);
            applyNumericFilter(tfTrack);
            applyNumericFilter(tfDisc);

            // Row 0
            gbc.gridx = 0; gbc.gridy = 0; fieldsPanel.add(new JLabel("Title:"), gbc);
            gbc.gridx = 1; gbc.weightx = 1.0; fieldsPanel.add(tfTitle, gbc);
            gbc.gridx = 2; gbc.weightx = 0;   fieldsPanel.add(new JLabel("Artist:"), gbc);
            gbc.gridx = 3; gbc.weightx = 1.0; fieldsPanel.add(tfArtist, gbc);
            
            // Row 1
            gbc.gridy = 1;
            gbc.gridx = 0; gbc.weightx = 0;   fieldsPanel.add(new JLabel("Album:"), gbc);
            gbc.gridx = 1; gbc.weightx = 1.0; fieldsPanel.add(tfAlbum, gbc);
            gbc.gridx = 2; gbc.weightx = 0;   fieldsPanel.add(new JLabel("Genre:"), gbc);
            gbc.gridx = 3; gbc.weightx = 1.0; fieldsPanel.add(cbGenre, gbc);
            
            // Row 2
            gbc.gridy = 2;
            gbc.gridx = 0; gbc.weightx = 0;   fieldsPanel.add(new JLabel("Year:"), gbc);
            gbc.gridx = 1; gbc.weightx = 1.0; fieldsPanel.add(tfYear, gbc);
            gbc.gridx = 2; gbc.weightx = 0;   fieldsPanel.add(new JLabel("Track:"), gbc);
            gbc.gridx = 3; gbc.weightx = 1.0; fieldsPanel.add(tfTrack, gbc);
            
            // Row 3
            gbc.gridy = 3;
            gbc.gridx = 0; gbc.weightx = 0;   fieldsPanel.add(new JLabel("Disc #:"), gbc);
            gbc.gridx = 1; gbc.weightx = 1.0; fieldsPanel.add(tfDisc, gbc);
            gbc.gridx = 2; gbc.weightx = 0;   fieldsPanel.add(new JLabel("Composer:"), gbc);
            gbc.gridx = 3; gbc.weightx = 1.0; fieldsPanel.add(tfComposer, gbc);
            
            // Row 4
            gbc.gridy = 4;
            gbc.gridx = 0; gbc.weightx = 0;   fieldsPanel.add(new JLabel("Comment:"), gbc);
            gbc.gridx = 1; gbc.gridwidth = 3; fieldsPanel.add(tfComment, gbc);

            JButton btnSave = new JButton("Save Changes");
            btnSave.setFont(new Font("SansSerif", Font.BOLD, 14));
            btnSave.setPreferredSize(new Dimension(150, 32));
            btnSave.setEnabled(false);
            
            gbc.gridy = 5; gbc.gridx = 3; gbc.gridwidth = 1;
            gbc.anchor = GridBagConstraints.EAST; gbc.fill = GridBagConstraints.NONE;
            fieldsPanel.add(btnSave, gbc);

            editPanel.add(infoPanel, BorderLayout.WEST);
            editPanel.add(fieldsPanel, BorderLayout.CENTER);
            setPanelEnabled(editPanel, false);

            lblCover.addMouseListener(new MouseAdapter() {
                @Override
                public void mouseClicked(MouseEvent e) {
                    if (lblCover.isEnabled()) {
                        System.setProperty("apple.awt.fileDialogForDirectories", "false");
                        FileDialog fd = new FileDialog(frame, "Select Cover Art", FileDialog.LOAD);
                        fd.setFilenameFilter((dir, name) -> {
                            String n = name.toLowerCase();
                            return n.endsWith(".jpg") || n.endsWith(".jpeg") || n.endsWith(".png");
                        });
                        fd.setVisible(true);
                        if (fd.getDirectory() != null && fd.getFile() != null) {
                            newCoverFile = new File(fd.getDirectory(), fd.getFile());
                            try {
                                BufferedImage img = ImageIO.read(newCoverFile);
                                lblCover.setIcon(new ImageIcon(ImageUtils.scaleImage(img, AudioFileProcessor.LARGE_COVER_SIZE, AudioFileProcessor.LARGE_COVER_SIZE)));
                                lblCover.setText("");
                            } catch (Exception ex) {
                                JOptionPane.showMessageDialog(frame, "Failed to load image.");
                            }
                        }
                    }
                }
            });

            // Save Action
            Runnable saveAction = () -> {
                int row = table.getSelectedRow();
                if (row != -1) {
                    int modelRow = table.convertRowIndexToModel(row);
                    String path = (String) model.getValueAt(modelRow, 11);
                    
                    String genre = (String) cbGenre.getEditor().getItem();

                    if (AudioFileProcessor.updateMetadata(path, tfTitle.getText(), tfArtist.getText(), tfAlbum.getText(), 
                            tfYear.getText(), tfTrack.getText(), genre, tfComment.getText(), 
                            tfComposer.getText(), tfDisc.getText(), newCoverFile)) {
                        newCoverFile = null;
                        AudioMetadata m = AudioFileProcessor.extractMetadata(new File(path));
                        
                        // Update Table
                        model.setValueAt(m.coverArt, modelRow, 0);
                        model.setValueAt(m.title, modelRow, 2);
                        model.setValueAt(m.artistName, modelRow, 3);
                        model.setValueAt(m.album, modelRow, 4);
                        model.setValueAt(m.year, modelRow, 5);
                        model.setValueAt(m.trackNumber, modelRow, 6);
                        model.setValueAt(m.genre, modelRow, 7);
                        model.setValueAt(m.discNumber, modelRow, 8);
                        model.setValueAt(m.composer, modelRow, 9);
                        model.setValueAt(m.comment, modelRow, 10);
                        
                        // Update Edit Panel Fields
                        tfTitle.setText(m.title);
                        tfArtist.setText(m.artistName);
                        tfAlbum.setText(m.album);
                        tfYear.setText(m.year);
                        tfTrack.setText(m.trackNumber);
                        cbGenre.setSelectedItem(m.genre != null ? m.genre : "");
                        if (!cbGenre.getSelectedItem().equals(m.genre)) {
                            cbGenre.getEditor().setItem(m.genre != null ? m.genre : "");
                        }
                        tfDisc.setText(m.discNumber);
                        tfComposer.setText(m.composer);
                        tfComment.setText(m.comment);
                        
                        lblCover.setIcon(m.largeCoverArt);
                        lblCover.setText(m.largeCoverArt == null ? "<html><center>No Cover<br>(Click to add)</center></html>" : "");
                        
                        table.repaint();
                        JOptionPane.showMessageDialog(frame, "Metadata saved successfully.");
                    } else {
                        JOptionPane.showMessageDialog(frame, "Failed to save metadata.", "Error", JOptionPane.ERROR_MESSAGE);
                    }
                }
            };

            btnSave.addActionListener(e -> saveAction.run());

            // Selection Listener
            table.getSelectionModel().addListSelectionListener(e -> {
                if (!e.getValueIsAdjusting()) {
                    int row = table.getSelectedRow();
                    boolean hasSelection = row != -1;
                    setPanelEnabled(editPanel, hasSelection);
                    btnSave.setEnabled(hasSelection);
                    if (saveMenuItem != null) saveMenuItem.setEnabled(hasSelection);
                    newCoverFile = null;

                    if (hasSelection) {
                        int modelRow = table.convertRowIndexToModel(row);
                        String path = (String) model.getValueAt(modelRow, 11);
                        AudioMetadata m = AudioFileProcessor.extractMetadata(new File(path));
                        lblEditTitle.setText(m.fileName + " - Edit");
                        lblCover.setIcon(m.largeCoverArt);
                        lblCover.setText(m.largeCoverArt == null ? "<html><center>No Cover<br>(Click to add)</center></html>" : "");
                        lblFileName.setText("File: " + m.fileName);
                        tfTitle.setText(m.title);
                        tfArtist.setText(m.artistName);
                        tfAlbum.setText(m.album);
                        tfYear.setText(m.year);
                        tfTrack.setText(m.trackNumber);
                        cbGenre.setSelectedItem(m.genre != null ? m.genre : "");
                        if (!cbGenre.getSelectedItem().equals(m.genre)) {
                            cbGenre.getEditor().setItem(m.genre != null ? m.genre : "");
                        }
                        tfDisc.setText(m.discNumber);
                        tfComposer.setText(m.composer);
                        tfComment.setText(m.comment);
                    } else {
                        lblEditTitle.setText("No file selected - Edit");
                        lblCover.setIcon(null);
                        lblCover.setText("");
                        lblFileName.setText("File: ");
                        tfTitle.setText("");
                        tfArtist.setText("");
                        tfAlbum.setText("");
                        tfYear.setText("");
                        tfTrack.setText("");
                        cbGenre.setSelectedItem("");
                        tfDisc.setText("");
                        tfComposer.setText("");
                        tfComment.setText("");
                    }
                }
            });

            // Shortcut Ctrl+S
            KeyStroke saveKeyStroke = KeyStroke.getKeyStroke(KeyEvent.VK_S, 
                System.getProperty("os.name").toLowerCase().contains("mac") ? InputEvent.META_DOWN_MASK : InputEvent.CTRL_DOWN_MASK);
            
            frame.getRootPane().getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW).put(saveKeyStroke, "save");
            frame.getRootPane().getActionMap().put("save", new AbstractAction() {
                @Override public void actionPerformed(java.awt.event.ActionEvent e) {
                    if (btnSave.isEnabled()) saveAction.run();
                }
            });

            // Assemble UI
            JToolBar toolBar = new JToolBar();
            toolBar.setFloatable(false);
            toolBar.setMargin(new Insets(5, 5, 5, 5));
            toolBar.add(createToolbarButton("Open Folder", UIManager.getIcon("FileView.directoryIcon"), new Dimension(130, 32), ev -> openFolderDialog(frame, model)));
            toolBar.add(createToolbarButton("Open File", UIManager.getIcon("FileView.fileIcon"), new Dimension(110, 32), ev -> openFileDialog(frame, model)));
            
            JButton btnRemove = createToolbarButton("Remove", UIManager.getIcon("InternalFrame.closeIcon"), new Dimension(110, 32), ev -> removeSelectedRows(table, model));
            btnRemove.setEnabled(false);
            toolBar.add(btnRemove);
            
            toolBar.addSeparator(new Dimension(15, 0));
            JButton btnShowInFinder = createToolbarButton("Show in Finder", UIManager.getIcon("FileView.hardDriveIcon"), new Dimension(145, 32), ev -> showSelectedInFinder(table, model));
            btnShowInFinder.setEnabled(false);
            toolBar.add(btnShowInFinder);

            table.getSelectionModel().addListSelectionListener(ev -> {
                if (!ev.getValueIsAdjusting()) {
                    boolean hasSelection = table.getSelectedRow() != -1;
                    btnRemove.setEnabled(hasSelection);
                    btnShowInFinder.setEnabled(hasSelection);
                }
            });

            frame.add(toolBar, BorderLayout.NORTH);
            
            JSplitPane splitPane = new JSplitPane(JSplitPane.VERTICAL_SPLIT, new JScrollPane(table), editPanel);
            splitPane.setDividerLocation(430);
            splitPane.setResizeWeight(1.0);
            frame.add(splitPane, BorderLayout.CENTER);
            
            setupMenuBar(frame, model, saveAction);
            frame.setLocationRelativeTo(null);
            frame.setVisible(true);
        });
    }

    private static void applyNumericFilter(JTextField field) {
        ((AbstractDocument) field.getDocument()).setDocumentFilter(new DocumentFilter() {
            @Override
            public void insertString(FilterBypass fb, int offset, String string, AttributeSet attr) throws BadLocationException {
                if (string != null && string.matches("\\d*")) super.insertString(fb, offset, string, attr);
            }
            @Override
            public void replace(FilterBypass fb, int offset, int length, String text, AttributeSet attrs) throws BadLocationException {
                if (text != null && text.matches("\\d*")) super.replace(fb, offset, length, text, attrs);
            }
        });
    }

    private static void setPanelEnabled(JPanel panel, boolean enabled) {
        for (Component cp : panel.getComponents()) {
            cp.setEnabled(enabled);
            if (cp instanceof JPanel) setPanelEnabled((JPanel) cp, enabled);
            if (cp instanceof JComboBox) cp.setEnabled(enabled);
        }
    }

    private static JTable createTable(DefaultTableModel model) {
        JTable table = new JTable(model) {
            @Override
            public Component prepareRenderer(TableCellRenderer r, int row, int col) {
                Component c = super.prepareRenderer(r, row, col);
                if (!isRowSelected(row)) c.setBackground(row % 2 == 0 ? Color.WHITE : new Color(240, 240, 240));
                return c;
            }

            @Override
            protected void paintComponent(Graphics g) {
                super.paintComponent(g);
                if (getModel().getRowCount() == 0) {
                    Graphics2D g2 = (Graphics2D) g.create();
                    g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
                    g2.setColor(Color.LIGHT_GRAY);
                    g2.setFont(new Font("SansSerif", Font.BOLD, 18));
                    String text = "Drag and drop audio files here or use the toolbar to get started.";
                    FontMetrics fm = g2.getFontMetrics();
                    g2.drawString(text, (getWidth() - fm.stringWidth(text)) / 2, (getHeight() / 2) + fm.getAscent() / 2);
                    g2.dispose();
                }
            }
        };
        table.setRowHeight(AudioFileProcessor.THUMBNAIL_SIZE + 6);
        table.setFillsViewportHeight(true);
        table.setAutoCreateRowSorter(true);
        table.setSelectionMode(ListSelectionModel.MULTIPLE_INTERVAL_SELECTION);
        table.getColumnModel().getColumn(0).setMaxWidth(AudioFileProcessor.THUMBNAIL_SIZE + 10);
        table.getColumnModel().getColumn(0).setMinWidth(AudioFileProcessor.THUMBNAIL_SIZE + 10);
        return table;
    }

    private static JButton createToolbarButton(String text, Icon icon, Dimension size, java.awt.event.ActionListener al) {
        JButton btn = new JButton(text, icon);
        btn.setVerticalTextPosition(SwingConstants.CENTER);
        btn.setHorizontalTextPosition(SwingConstants.TRAILING);
        btn.setFocusable(false);
        btn.setPreferredSize(size);
        btn.setMinimumSize(size);
        btn.setMaximumSize(size);
        btn.setFont(new Font("SansSerif", Font.PLAIN, 12));
        if (al != null) btn.addActionListener(al);
        return btn;
    }

    private static void openFolderDialog(JFrame frame, DefaultTableModel model) {
        System.setProperty("apple.awt.fileDialogForDirectories", "true");
        FileDialog d = new FileDialog(frame, "Select Folder", FileDialog.LOAD);
        d.setVisible(true);
        if (d.getDirectory() != null) {
            File f = new File(d.getDirectory(), d.getFile() != null ? d.getFile() : "");
            if (f.exists()) {
                recentManager.addToRecent(f.getAbsolutePath());
                loadFiles(Collections.singletonList(f), model);
                updateRecentMenu(model);
            }
        }
    }

    private static void openFileDialog(JFrame frame, DefaultTableModel model) {
        System.setProperty("apple.awt.fileDialogForDirectories", "false");
        FileDialog d = new FileDialog(frame, "Select Audio File", FileDialog.LOAD);
        d.setFilenameFilter((dir, name) -> AudioFileProcessor.isAudioFile(new File(dir, name)));
        d.setVisible(true);
        if (d.getDirectory() != null && d.getFile() != null) {
            loadFiles(Collections.singletonList(new File(d.getDirectory(), d.getFile())), model);
        }
    }

    private static void removeSelectedRows(JTable table, DefaultTableModel model) {
        int[] rows = table.getSelectedRows();
        for (int i = rows.length - 1; i >= 0; i--) {
            int modelRow = table.convertRowIndexToModel(rows[i]);
            loadedFilePaths.remove((String) model.getValueAt(modelRow, 11));
            model.removeRow(modelRow);
        }
        table.repaint();
    }

    private static void showSelectedInFinder(JTable table, DefaultTableModel model) {
        int row = table.getSelectedRow();
        if (row != -1) {
            File f = new File((String) model.getValueAt(table.convertRowIndexToModel(row), 11));
            try {
                if (Desktop.isDesktopSupported()) Desktop.getDesktop().browseFileDirectory(f);
                else Runtime.getRuntime().exec(new String[]{"open", "-R", f.getAbsolutePath()});
            } catch (Exception ignored) {}
        }
    }

    private static void setupMenuBar(JFrame frame, DefaultTableModel model, Runnable saveAction) {
        JMenuBar mb = new JMenuBar();
        JMenu fm = new JMenu("File");
        
        JMenuItem openFolder = new JMenuItem("Open Folder...", UIManager.getIcon("FileView.directoryIcon"));
        openFolder.addActionListener(e -> openFolderDialog(frame, model));

        JMenuItem openFile = new JMenuItem("Open File...", UIManager.getIcon("FileView.fileIcon"));
        openFile.addActionListener(e -> openFileDialog(frame, model));

        saveMenuItem = new JMenuItem("Save Changes");
        saveMenuItem.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_S, 
            System.getProperty("os.name").toLowerCase().contains("mac") ? InputEvent.META_DOWN_MASK : InputEvent.CTRL_DOWN_MASK));
        saveMenuItem.addActionListener(e -> saveAction.run());
        saveMenuItem.setEnabled(false);

        recentMenu = new JMenu("Open Recent");
        updateRecentMenu(model);

        fm.add(openFolder);
        fm.add(openFile);
        fm.add(saveMenuItem);
        fm.addSeparator();
        fm.add(recentMenu);
        mb.add(fm);
        frame.setJMenuBar(mb);
    }

    private static void updateRecentMenu(DefaultTableModel model) {
        recentMenu.removeAll();
        List<String> paths = recentManager.getRecentFolders();
        if (paths.isEmpty()) { recentMenu.setEnabled(false); return; }
        recentMenu.setEnabled(true);
        for (String p : paths) {
            JMenuItem item = new JMenuItem(p);
            item.addActionListener(e -> loadFiles(Collections.singletonList(new File(p)), model));
            recentMenu.add(item);
        }
    }

    private static void loadFiles(List<File> files, DefaultTableModel model) {
        new Thread(() -> {
            for (File f : files) {
                if (f.isDirectory()) {
                    for (File audio : AudioFileProcessor.scanDirectory(f)) processFile(audio, model);
                } else if (AudioFileProcessor.isAudioFile(f)) {
                    processFile(f, model);
                }
            }
        }).start();
    }

    private static void processFile(File file, DefaultTableModel model) {
        if (!loadedFilePaths.add(file.getAbsolutePath())) return;
        AudioMetadata m = AudioFileProcessor.extractMetadata(file);
        SwingUtilities.invokeLater(() -> {
            model.addRow(new Object[]{m.coverArt, m.fileName, m.title, m.artistName, m.album, m.year, m.trackNumber, m.genre, m.discNumber, m.composer, m.comment, m.filePath});
        });
    }
}

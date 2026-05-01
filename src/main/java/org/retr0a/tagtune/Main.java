package org.retr0a.tagtune;

import javax.imageio.ImageIO;
import javax.swing.*;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.TableCellRenderer;
import java.awt.*;
import java.awt.datatransfer.DataFlavor;
import java.awt.event.InputEvent;
import java.awt.event.KeyEvent;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.image.BufferedImage;
import java.io.File;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.List;
import java.util.function.Consumer;

public class Main {
    private static final Set<String> loadedFilePaths = Collections.synchronizedSet(new HashSet<>());
    private static final RecentFoldersManager recentManager = new RecentFoldersManager();
    private static JMenu recentMenu;
    private static JMenuItem saveMenuItem;
    private static final SimpleDateFormat DATE_FORMAT = new SimpleDateFormat("yyyy-MM-dd");
    private static File newCoverFile = null;

    public static void main(String[] args) {
        System.setProperty("apple.awt.application.name", "TagTune");
        System.setProperty("apple.laf.useScreenMenuBar", "true");
        System.setProperty("com.apple.mrj.application.apple.menu.about.name", "TagTune");
        System.setProperty("apple.awt.fileDialogForDirectories", "true");

        SwingUtilities.invokeLater(() -> {
            JFrame frame = new JFrame("TagTune");
            frame.setSize(1000, 750);
            frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            frame.setLayout(new BorderLayout());

            Image appIcon = ImageUtils.createAppIcon(512);
            frame.setIconImage(appIcon);
            if (Taskbar.isTaskbarSupported()) {
                try { Taskbar.getTaskbar().setIconImage(appIcon); } catch (Exception ignored) {}
            }

            // Table Model
            String[] columnNames = {"Cover", "File Name", "Title", "Artist Name", "Release Date", "Path"};
            DefaultTableModel model = new DefaultTableModel(columnNames, 0) {
                @Override public boolean isCellEditable(int row, int col) { return false; }
                @Override public Class<?> getColumnClass(int col) { return col == 0 ? ImageIcon.class : Object.class; }
            };

            // Table UI
            JTable table = createTable(model);
            table.getColumnModel().removeColumn(table.getColumnModel().getColumn(5)); // Hide Path

            // Edit Panel
            JPanel editPanel = new JPanel(new BorderLayout(15, 15));
            editPanel.setBorder(BorderFactory.createEmptyBorder(10, 20, 10, 20));
            
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
            infoPanel.add(Box.createVerticalStrut(5));
            infoPanel.add(lblFileName);

            JPanel fieldsPanel = new JPanel(new GridBagLayout());
            GridBagConstraints gbc = new GridBagConstraints();
            gbc.fill = GridBagConstraints.HORIZONTAL;
            gbc.insets = new Insets(5, 8, 5, 8);

            JTextField tfTitle = new JTextField(30);
            JTextField tfArtist = new JTextField(30);
            
            JTextField tfDate = new JTextField(20);
            tfDate.setEditable(false);
            JButton btnCalendar = new JButton("📅");
            
            JPanel datePanel = new JPanel(new BorderLayout(5, 0));
            datePanel.add(tfDate, BorderLayout.CENTER);
            datePanel.add(btnCalendar, BorderLayout.EAST);

            gbc.gridx = 0; gbc.gridy = 0; fieldsPanel.add(new JLabel("Title:"), gbc);
            gbc.gridx = 1; fieldsPanel.add(tfTitle, gbc);
            gbc.gridy = 1; gbc.gridx = 0; fieldsPanel.add(new JLabel("Artist:"), gbc);
            gbc.gridx = 1; fieldsPanel.add(tfArtist, gbc);
            gbc.gridy = 2; gbc.gridx = 0; fieldsPanel.add(new JLabel("Release Date:"), gbc);
            gbc.gridx = 1; fieldsPanel.add(datePanel, gbc);

            JButton btnSave = new JButton("Save Changes");
            btnSave.setFont(new Font("SansSerif", Font.BOLD, 13));
            btnSave.setEnabled(false);
            gbc.gridy = 3; gbc.gridx = 1; gbc.anchor = GridBagConstraints.EAST; gbc.fill = GridBagConstraints.NONE;
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

            btnCalendar.addActionListener(e -> {
                Date current;
                try { current = DATE_FORMAT.parse(tfDate.getText()); } 
                catch (Exception ex) { current = new Date(); }
                showDatePicker(frame, current, d -> tfDate.setText(DATE_FORMAT.format(d)));
            });

            // Save Action
            Runnable saveAction = () -> {
                int row = table.getSelectedRow();
                if (row != -1) {
                    int modelRow = table.convertRowIndexToModel(row);
                    String path = (String) model.getValueAt(modelRow, 5);
                    String titleText = tfTitle.getText();
                    String artistText = tfArtist.getText();
                    String dateText = tfDate.getText();
                    
                    if (AudioFileProcessor.updateMetadata(path, titleText, artistText, dateText, newCoverFile)) {
                        newCoverFile = null;
                        // Refresh data in table
                        AudioMetadata m = AudioFileProcessor.extractMetadata(new File(path));
                        model.setValueAt(m.coverArt, modelRow, 0);
                        model.setValueAt(m.title, modelRow, 2);
                        model.setValueAt(m.artistName, modelRow, 3);
                        model.setValueAt(m.releaseDate, modelRow, 4);
                        
                        // Sync edit panel too
                        lblCover.setIcon(m.largeCoverArt);
                        lblCover.setText(m.largeCoverArt == null ? "<html><center>No Cover<br>(Click to add)</center></html>" : "");
                        
                        table.repaint();
                        JOptionPane.showMessageDialog(frame, "Metadata saved successfully.");
                    } else {
                        JOptionPane.showMessageDialog(frame, "Failed to save metadata. Ensure the file format supports the chosen fields.", "Error", JOptionPane.ERROR_MESSAGE);
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
                        String path = (String) model.getValueAt(modelRow, 5);
                        AudioMetadata m = AudioFileProcessor.extractMetadata(new File(path));
                        lblCover.setIcon(m.largeCoverArt);
                        lblCover.setText(m.largeCoverArt == null ? "<html><center>No Cover<br>(Click to add)</center></html>" : "");
                        lblFileName.setText("File: " + m.fileName);
                        tfTitle.setText(m.title);
                        tfArtist.setText(m.artistName);
                        tfDate.setText(m.releaseDate != null ? m.releaseDate : "");
                    } else {
                        lblCover.setIcon(null);
                        lblCover.setText("");
                        lblFileName.setText("File: ");
                        tfTitle.setText("");
                        tfArtist.setText("");
                        tfDate.setText("");
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
            splitPane.setDividerLocation(520);
            splitPane.setResizeWeight(1.0);
            frame.add(splitPane, BorderLayout.CENTER);
            
            setupMenuBar(frame, model, saveAction);
            frame.setLocationRelativeTo(null);
            frame.setVisible(true);
        });
    }

    private static void setPanelEnabled(JPanel panel, boolean enabled) {
        for (Component cp : panel.getComponents()) {
            cp.setEnabled(enabled);
            if (cp instanceof JPanel) setPanelEnabled((JPanel) cp, enabled);
        }
    }

    private static void showDatePicker(JFrame parent, Date initialDate, Consumer<Date> onSelect) {
        JDialog dialog = new JDialog(parent, "Select Date", true);
        dialog.setResizable(false);
        dialog.setLayout(new BorderLayout());
        
        Calendar cal = Calendar.getInstance();
        cal.setTime(initialDate != null ? initialDate : new Date());
        
        JPanel calendarPanel = new JPanel(new BorderLayout());
        JLabel monthLabel = new JLabel("", SwingConstants.CENTER);
        monthLabel.setFont(new Font("SansSerif", Font.BOLD, 13));
        
        JPanel daysGrid = new JPanel(new GridLayout(0, 7));
        
        Runnable updateDays = () -> {
            daysGrid.removeAll();
            monthLabel.setText(new SimpleDateFormat("MMMM yyyy").format(cal.getTime()));
            
            String[] headers = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
            for (String h : headers) {
                JLabel l = new JLabel(h, SwingConstants.CENTER);
                l.setFont(new Font("SansSerif", Font.PLAIN, 10));
                l.setForeground(Color.GRAY);
                daysGrid.add(l);
            }
            
            Calendar temp = (Calendar) cal.clone();
            temp.set(Calendar.DAY_OF_MONTH, 1);
            int startDay = temp.get(Calendar.DAY_OF_WEEK);
            for (int i = 1; i < startDay; i++) daysGrid.add(new JLabel(""));
            
            int maxDay = temp.getActualMaximum(Calendar.DAY_OF_MONTH);
            for (int i = 1; i <= maxDay; i++) {
                int day = i;
                JButton btn = new JButton(String.valueOf(i));
                btn.setFont(new Font("SansSerif", Font.PLAIN, 10));
                btn.setMargin(new Insets(1, 1, 1, 1));
                btn.addActionListener(e -> {
                    cal.set(Calendar.DAY_OF_MONTH, day);
                    onSelect.accept(cal.getTime());
                    dialog.dispose();
                });
                daysGrid.add(btn);
            }
            daysGrid.revalidate();
            daysGrid.repaint();
        };

        JButton btnPrev = new JButton("<");
        btnPrev.setPreferredSize(new Dimension(40, 25));
        btnPrev.addActionListener(e -> { cal.add(Calendar.MONTH, -1); updateDays.run(); });
        JButton btnNext = new JButton(">");
        btnNext.setPreferredSize(new Dimension(40, 25));
        btnNext.addActionListener(e -> { cal.add(Calendar.MONTH, 1); updateDays.run(); });
        
        JPanel header = new JPanel(new BorderLayout());
        header.add(btnPrev, BorderLayout.WEST);
        header.add(monthLabel, BorderLayout.CENTER);
        header.add(btnNext, BorderLayout.EAST);
        
        calendarPanel.add(header, BorderLayout.NORTH);
        calendarPanel.add(daysGrid, BorderLayout.CENTER);
        
        updateDays.run();
        dialog.add(calendarPanel);
        dialog.pack();
        dialog.setSize(250, 230);
        dialog.setLocationRelativeTo(parent);
        dialog.setVisible(true);
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
            loadedFilePaths.remove((String) model.getValueAt(modelRow, 5));
            model.removeRow(modelRow);
        }
        table.repaint();
    }

    private static void showSelectedInFinder(JTable table, DefaultTableModel model) {
        int row = table.getSelectedRow();
        if (row != -1) {
            File f = new File((String) model.getValueAt(table.convertRowIndexToModel(row), 5));
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
            model.addRow(new Object[]{m.coverArt, m.fileName, m.title, m.artistName, m.releaseDate, m.filePath});
        });
    }
}

import sys

from PySide6.QtWidgets import QApplication

from app.ui.main_window import MainWindow
from app.ui.icon_helper import app_icon

class Application:
    def __init__(self):
        self.qt_app = QApplication(sys.argv)

        self.qt_app.setApplicationName("TagTune")
        self.qt_app.setApplicationDisplayName("TagTune")
        self.qt_app.setOrganizationName("Retr0A")
        self.qt_app.setWindowIcon(app_icon())

        self.window = MainWindow()

    def run(self):
        self.window.show()
        sys.exit(self.qt_app.exec())
        

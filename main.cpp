#include <QApplication>
#include <QCoreApplication>

#include "src/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application identity for QSettings (recent files/folders)
    QCoreApplication::setOrganizationName("TagTuneOrg");
    QCoreApplication::setApplicationName("TagTune");

    MainWindow window;
    window.show();

    return QApplication::exec();
}
#include "app.h"
#include <QApplication>
#include <QFile>
#include <QFontDatabase>


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setOrganizationName("z2z63");
    QApplication::setOrganizationDomain("z2z63.dev");
    QApplication::setApplicationName("skipper");

    QFile fontFile(":/fonts/src/fonts/lishu.ttf");
    if (fontFile.open(QIODevice::ReadOnly)) {
        QFontDatabase::addApplicationFontFromData(fontFile.readAll());
    }

    App systemTrayApp;
    return QApplication::exec();
}
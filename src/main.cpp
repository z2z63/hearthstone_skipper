#include "tray_icon.h"
#include <QApplication>



int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    TrayIcon systemTrayApp;
    return QApplication::exec();
}
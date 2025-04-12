#include "app.h"
#include <QApplication>


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setOrganizationName("z2z63");
    QApplication::setOrganizationDomain("z2z63.dev");
    QApplication::setApplicationName("skipper");
    App systemTrayApp;
    return QApplication::exec();
}
#include "tray_icon.h"


#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QDesktopServices>
#include <QDir>


TrayIcon::TrayIcon(QObject *parent)
    : QObject(parent), skipper(Skipper::create()) {
    createActions();
    createTrayIcon();
    trayIcon->show();
}

TrayIcon::~TrayIcon() {
    delete trayIcon;
    delete trayIconMenu;
}

void TrayIcon::createActions() {
    function1Action = new QAction(tr("一键拔线"), this);
    connect(function1Action, &QAction::triggered, this, &TrayIcon::onFunction1);

    function2Action = new QAction(tr("打开日志"), this);
    connect(function2Action, &QAction::triggered, this, &TrayIcon::onFunction2);

    quitAction = new QAction(tr("退出"), this);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

void TrayIcon::createTrayIcon() {
    trayIconMenu = new QMenu();
    trayIconMenu->addAction(function1Action);
    trayIconMenu->addAction(function2Action);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);

    trayIcon->setIcon(QIcon::fromTheme("dialog-information"));
    trayIcon->setContextMenu(trayIconMenu);

}

void TrayIcon::onFunction1() {
    skipper->skip();
}

void TrayIcon::onFunction2() {
    // 实现功能2的逻辑
    QDesktopServices::openUrl(QUrl("file:///" + QDir::homePath() + "/.hearthstone_skipper/log.txt"));
}
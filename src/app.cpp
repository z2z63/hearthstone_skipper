#include "app.h"

#include "logger.h"
#include "setting.h"


#include <QApplication>
#include <QTimer>
#include <QDesktopServices>
#include <QDir>
#include <QDialog>
#include <QUrl>

std::shared_ptr<Skipper> App::skipper;

ConfigState App::configState;

App::App(QObject *parent)
    : QObject(parent) {
    initLogger();
    manager = std::make_unique<QNetworkAccessManager>(this);
    skipper = std::make_shared<Skipper>();
    configState =  skipper->tryConfig();
    createActions();
    createTrayIcon();
    trayIcon->show();
    settingDialog = nullptr;
    onFunction3();
}

App::~App() {
    delete trayIcon;
    delete trayIconMenu;
    if (settingDialog) {
        delete settingDialog;
    }
}

void App::createActions() {
    function1Action = new QAction(tr("一键拔线"), this);
    connect(function1Action, &QAction::triggered, this, &App::onFunction1);

    function2Action = new QAction(tr("打开日志"), this);
    connect(function2Action, &QAction::triggered, this, &App::onFunction2);

    function3Action = new QAction(tr("打开设置"), this);
    connect(function3Action, &QAction::triggered, this, &App::onFunction3);

    quitAction = new QAction(tr("退出"), this);
    connect(quitAction, &QAction::triggered, this, &QApplication::quit);
}

void App::createTrayIcon() {
    trayIconMenu = new QMenu();
    trayIconMenu->addAction(function1Action);
    trayIconMenu->addAction(function2Action);
    trayIconMenu->addAction(function3Action);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);

    trayIcon->setIcon(QIcon::fromTheme("dialog-information"));
    trayIcon->setContextMenu(trayIconMenu);

}

void App::onFunction1() {
    if (skipper) {
        skipper->skip();
    }
}

void App::onFunction2() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(getLogFilePath()));
}

void App::onFunction3() {
    if (settingDialog == nullptr) {
        settingDialog = new SettingDialog();
    }
    settingDialog->show();
}
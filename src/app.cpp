#include "app.h"

#include "logger.h"
#include "setting_dialog.h"
#include "skipper.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDialog>
#include <QUrl>

std::shared_ptr<Skipper> App::skipper;

QCurlEasy *App::curlEasy;

App::App(QObject *parent)
    : QObject(parent), trayIcon(nullptr) {
    initLogger();
    const AppSettings &settings = AppSettings::instance();
    skipper = std::make_shared<Skipper>(ClashConfig{.config_deduce_source = ExternalControllerType::TCPIP,
                                                    .external_controller = settings.external_controller().toStdString(),
                                                    .secret = settings.secret().toStdString(),
                                                    .unix_socket = settings.unix_socket().toStdString()});
    settingDialog = new SettingDialog();

    createActions();
    createTrayIcon();
    assert(trayIcon != nullptr);
    trayIcon->show();
    onFunction3();
}

App::~App() {
    delete trayIcon;
    delete trayIconMenu;
    delete settingDialog;
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
    settingDialog->show();
}
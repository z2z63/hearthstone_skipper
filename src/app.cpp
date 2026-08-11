#include "app.h"

#include "logger.h"
#include "setting_dialog.h"
#include "skipper.h"

#include <QApplication>
#include <QMainWindow>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QSysInfo>

Skipper *App::skipper;
App *App::_instance = nullptr;


App::App(QObject *parent) : QObject(parent), trayIcon(nullptr) {
    _instance = this;
    initLogger();
    const auto startupLogger = spdlog::get("skipper");
    SPDLOG_LOGGER_INFO(startupLogger, "application_start version={} qt={} os={} arch={} pid={} log={}", APP_VERSION,
                       QT_VERSION_STR, QSysInfo::prettyProductName().toStdString(),
                       QSysInfo::currentCpuArchitecture().toStdString(), QCoreApplication::applicationPid(),
                       getLogFilePath().toStdString());
    if (std::optional clash_config = AppSettings::instance().clash_config(); !clash_config.has_value()) {
        qeasy = new ConfigAwareQEasy({}, this);
        auto *deducer = new ConfigDeducer(qeasy, this);
        skipper = new Skipper(qeasy, this);
        deducer->tryDeduce();
        connect(deducer, &ConfigDeducer::deduceFinished, this, [this](ConfigAwareQEasy *deduced) {
            if (deduced == nullptr) {
                return;
            }
            if (settingDialog) {
                settingDialog->setting_tab->setHitText("skipper 设置已自动推断");
            }
            AppSettings::instance().clash_config_set(deduced->config());
        });
    } else {
        qeasy = new ConfigAwareQEasy(clash_config.value(), this);
        skipper = new Skipper(qeasy, this);
    }
    settingDialog = new SettingDialog();

    createActions();
    createTrayIcon();
    if (AppSettings::instance().float_button_enabled()) {
        floatButton = new FloatButton();
    }
    assert(trayIcon != nullptr);
    trayIcon->show();
    settingDialog->show();
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

void App::setFloatButtonEnabled(bool enabled) {
    AppSettings::instance().float_button_enabled_set(enabled);
    if (enabled && floatButton == nullptr) {
        floatButton = new FloatButton();
    } else if (!enabled && floatButton != nullptr) {
        delete floatButton;
        floatButton = nullptr;
    }
}

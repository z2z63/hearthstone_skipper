#pragma once



#include <QSystemTrayIcon>
#include <QMenu>
#include "skipper.h"
#include "config.h"

class SettingDialog;

class App : public QObject
{
    Q_OBJECT

public:
    explicit App(QObject* parent = nullptr);

    ~App() override;

private slots:
    void onFunction1();

    void onFunction2();

    void onFunction3();

private:
    void createActions();

    void createTrayIcon();

public:
    static std::shared_ptr<Skipper> skipper;
    static ConfigState configState;
private:

    QSystemTrayIcon* trayIcon;
    QMenu* trayIconMenu;

    QAction* function1Action;
    QAction* function2Action;
    QAction* function3Action;
    QAction* quitAction;
    SettingDialog* settingDialog;
};


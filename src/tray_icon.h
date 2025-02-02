#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include "skipper.h"

class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject* parent = nullptr);

    ~TrayIcon() override;

private slots:
    void onFunction1();

    void onFunction2();

private:
    void createActions();

    void createTrayIcon();

    QSystemTrayIcon* trayIcon;
    QMenu* trayIconMenu;

    QAction* function1Action;
    QAction* function2Action;
    QAction* quitAction;
    std::unique_ptr<Skipper> skipper;
};

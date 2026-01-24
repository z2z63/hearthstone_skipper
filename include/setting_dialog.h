#pragma once

#include "skipper.h"
#include "app_settings.h"

#include <QtGui/QWindow>
#include <QDialog>
#include <QTabWidget>
#include <QBoxLayout>


class SettingDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SettingDialog();
    ~SettingDialog() override;
};

class SettingTab final : public QWidget {
    Q_OBJECT

public:
    explicit SettingTab(QWidget *parent = nullptr);

    ~SettingTab() override;

private slots:
    void onClashConfigChange(const QString &external_controller, const QString &secret);

private:
    QBoxLayout *layout1;
    ClashConfig _config;
    QTimer *timer;

};

class AboutTab final : public QWidget {
    Q_OBJECT

public:
    explicit AboutTab(QWidget *parent = nullptr);
    ~AboutTab() override;

private:
    QBoxLayout *layout1;
};
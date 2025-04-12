#pragma once

#include "skipper.h"

#include <QtGui/QWindow>
#include <QDialog>
#include <QTabWidget>
#include <QBoxLayout>


class SettingDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingDialog();
    ~SettingDialog() override;
};

class SettingTab : public QWidget {
    Q_OBJECT

public:
    SettingTab(QWidget* parent = nullptr);

    ~SettingTab() override;

private slots:
    void onClashConfigChange(const QString &external_controller, const QString &secret);

private:
    QBoxLayout *layout1;
    ClashConfig _config;

};

class AboutTab : public QWidget {
    Q_OBJECT

public:
    AboutTab(QWidget* parent = nullptr);
    ~AboutTab() override;

private:
    QBoxLayout *layout1;
};
#pragma once

#include "skipper.h"
#include "app_settings.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QStatusBar>
#include <QStatusBar>
#include <QtGui/QWindow>

class SettingTab final : public QWidget {
    Q_OBJECT

public:
    explicit SettingTab(QWidget *parent = nullptr);

    ~SettingTab() override;

    void setHitText(const QString& text) const;

private slots:
    void onClashConfigChange(const QString &external_controller, const QString &secret);
private:
    void setupVisibility() const;
    void onTestBtnClicked();
    void onFormComplete() const;

private:
    QBoxLayout *layout_outer;
    QFormLayout* form_layout;
    QLineEdit *external_controller_edit;
    QLineEdit* secret_edit;
    QLineEdit* unix_socket_edit;
    QComboBox* external_controller_type_edit;
    QLabel* hint_label;
    QPushButton* test_btn;
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

class SettingDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SettingDialog();
    ~SettingDialog() override;

public:
    SettingTab* setting_tab;
    AboutTab* about_tab;
};
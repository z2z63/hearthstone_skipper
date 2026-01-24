#include "setting_dialog.h"

#include "app.h"

#include <QFormLayout>
#include <QtWidgets/qlabel.h>
#include <QTabWidget>
#include <QLineEdit>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#ifndef APP_VERSION
#define APP_VERSION "v0.0.0"
#endif

SettingDialog::SettingDialog() {
    setWindowTitle("设置");
    auto layout = new QVBoxLayout(this);
    setLayout(layout);

    auto *tabWidget = new QTabWidget();
    layout->addWidget(tabWidget);
    tabWidget->addTab(new SettingTab(), "设置");
    tabWidget->addTab(new AboutTab(), "关于");
    setWindowFlag(Qt::WindowStaysOnTopHint);
}

SettingDialog::~SettingDialog() = default;

SettingTab::SettingTab(QWidget *parent) : QWidget(parent), timer(new QTimer(parent)) {
    if (App::skipper) {
        _config = App::skipper->config();
    }
    layout1 = new QVBoxLayout();

    auto layout2 = new QFormLayout();
    auto lineEdit1 = new QLineEdit(QString::fromStdString(_config.external_controller));
    lineEdit1->setMinimumWidth(180);
    layout2->addRow("external controller", lineEdit1);
    auto lineEdit2 = new QLineEdit(QString::fromStdString(_config.secret));
    lineEdit2->setMinimumWidth(180);

    layout2->addRow("secret", lineEdit2);
    connect(lineEdit1, &QLineEdit::textEdited, this, [this](const QString &text) {
        _config.external_controller = text.toStdString();
    });
    connect(lineEdit2, &QLineEdit::textEdited, this, [this](const QString &text) {
        _config.secret = text.toStdString();
    });
    auto configStateHint = new QLabel();
    configStateHint->setMinimumHeight(0);
    auto onFormComplete = [this, configStateHint] {
        AppSettings settings;
        settings.external_controller_set(QString::fromStdString(_config.external_controller));
        settings.secret_set(QString::fromStdString(_config.secret));
        App::skipper->setConfig(_config);
        configStateHint->setText("已保存到设置");
        timer->start(3000);
        timer->callOnTimeout([configStateHint] {
            configStateHint->clear();
        });
    };
    connect(lineEdit1, &QLineEdit::editingFinished, this, onFormComplete);
    connect(lineEdit2, &QLineEdit::editingFinished, this, onFormComplete);

    auto btn1 = new QPushButton("检测");
    btn1->setMaximumWidth(80);
    connect(App::skipper.get(), &Skipper::testFinished, this, [this](bool ok) {
        if (ok) {
            QMessageBox::information(this, "提示", "检测成功");
        } else {
            QMessageBox::information(this, "提示", "检测失败");
        }
    });
    connect(btn1, &QPushButton::clicked, this, [this, lineEdit1, lineEdit2] {
        lineEdit1->clearFocus();
        lineEdit2->clearFocus();
        if (App::skipper) {
            App::skipper->test();
        } else {
            QMessageBox::warning(this, "错误", "无法创建 Skipper 实例");
        }
    });
    layout1->addLayout(layout2);
    layout1->addWidget(configStateHint, 0, Qt::AlignHCenter);
    layout1->addWidget(btn1, 0, Qt::AlignHCenter);
    setLayout(layout1);
}

SettingTab::~SettingTab() = default;

void SettingTab::onClashConfigChange(const QString &external_controller, const QString &secret) {
    if (!external_controller.isEmpty()) {
        _config.external_controller = external_controller.toStdString();
    }
    if (!secret.isEmpty()) {
        _config.secret = secret.toStdString();
    }
}

AboutTab::AboutTab(QWidget *parent) : QWidget(parent) {
    layout1 = new QVBoxLayout();
    layout1->addWidget(new QLabel(
        QString("<strong>Skipper") + APP_VERSION +
        "<br/>build with Qt" + QT_VERSION_STR
        "</strong><br/><a href='https://github.com/z2z63/hearthstone_skipper'>https://github.com/z2z63/hearthstone_skipper<a/>"));
    setLayout(layout1);
}

AboutTab::~AboutTab() = default;
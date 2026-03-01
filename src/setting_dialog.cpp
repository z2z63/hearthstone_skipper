#include "setting_dialog.h"

#include "app.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtWidgets/qlabel.h>

#ifndef APP_VERSION
#define APP_VERSION "v0.0.0"
#endif

SettingDialog::SettingDialog():setting_tab(new SettingTab), about_tab(new AboutTab) {
    setWindowTitle("设置");
    auto layout = new QVBoxLayout();
    setLayout(layout);

    auto *tabWidget = new QTabWidget();
    layout->addWidget(tabWidget);
    tabWidget->addTab(setting_tab, "设置");
    tabWidget->addTab(about_tab, "关于");
    setWindowFlag(Qt::WindowStaysOnTopHint);
}

SettingDialog::~SettingDialog() = default;

SettingTab::SettingTab(QWidget *parent) : QWidget(parent), _config(App::skipper->config()), timer(new QTimer(parent)) {
    layout_outer = new QVBoxLayout(this);
    form_layout = new QFormLayout();

    constexpr int IDX_TCPIP = 0;
    constexpr int IDX_UNIX = 1;
    external_controller_type_edit = new QComboBox(this);
    external_controller_type_edit->addItem("TCP/IP", "TCPIP");
    external_controller_type_edit->addItem("UNIX套接字", "UNIX_DOMAIN");
    if (_config.external_controller_type == ExternalControllerType::TCPIP) {
        external_controller_type_edit->setCurrentIndex(IDX_TCPIP);
    } else if (_config.external_controller_type == ExternalControllerType::UNIX_DOMAIN) {
        external_controller_type_edit->setCurrentIndex(IDX_UNIX);
    }

    external_controller_edit = new QLineEdit(QString::fromStdString(_config.external_controller), this);
    external_controller_edit->setMinimumWidth(180);
    secret_edit = new QLineEdit(QString::fromStdString(_config.secret), this);
    secret_edit->setMinimumWidth(180);

    unix_socket_edit = new QLineEdit(QString::fromStdString(_config.unix_socket), this);
    unix_socket_edit->setMinimumWidth(180);
    connect(external_controller_edit, &QLineEdit::textEdited, this,
            [this](const QString &text) { _config.external_controller = text.toStdString(); });
    connect(secret_edit, &QLineEdit::textEdited, this,
            [this](const QString &text) { _config.secret = text.toStdString(); });
    connect(unix_socket_edit, &QLineEdit::textEdited, this,
            [this](const QString &text) { _config.unix_socket = text.toStdString(); });
    hint_label = new QLabel(this);
    hint_label->setMinimumHeight(0);
    connect(external_controller_edit, &QLineEdit::editingFinished, this, &SettingTab::onFormComplete);
    connect(secret_edit, &QLineEdit::editingFinished, this, &SettingTab::onFormComplete);
    connect(unix_socket_edit, &QLineEdit::editingFinished, this, &SettingTab::onFormComplete);
    connect(external_controller_type_edit, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index == IDX_TCPIP) {
            _config.external_controller_type = ExternalControllerType::TCPIP;
        } else if (index == IDX_UNIX) {
            _config.external_controller_type = ExternalControllerType::UNIX_DOMAIN;
        }
        onFormComplete();
        setupVisibility();
    });

    test_btn = new QPushButton("检测", this);
    test_btn->setMaximumWidth(80);

    connect(test_btn, &QPushButton::clicked, this, &SettingTab::onTestBtnClicked);
    layout_outer->addLayout(form_layout);
    layout_outer->addWidget(hint_label, 0, Qt::AlignHCenter);
    layout_outer->addWidget(test_btn, 0, Qt::AlignHCenter);
    form_layout->addRow("外部控制器类型", external_controller_type_edit);
    form_layout->addRow("外部控制器", external_controller_edit);
    form_layout->addRow("密钥", secret_edit);
    form_layout->addRow("套接字文件", unix_socket_edit);
    setupVisibility();
}

void SettingTab::onFormComplete() const {
    auto &settings = AppSettings::instance();
    settings.clash_config_set(_config);
    App::skipper->setConfig(_config);
    hint_label->setText("已保存到设置");
    timer->start(1000);
    timer->callOnTimeout([this] { hint_label->clear(); });
}

void SettingTab::onTestBtnClicked() {
    onFormComplete();
    connect(
        App::skipper, &Skipper::testFinished, this,
        [this](bool ok) {
            if (ok) {
                QMessageBox::information(this, "提示", "检测成功");
            } else {
                QMessageBox::information(this, "提示", "检测失败");
            }
        },
        Qt::SingleShotConnection);
    App::skipper->test();
}

void SettingTab::setHitText(const QString &text) const {
    hint_label->clear();
    hint_label->setText(text);
    timer->start(3000);
    timer->callOnTimeout([this] { hint_label->clear(); });
}

SettingTab::~SettingTab() = default;

void SettingTab::setupVisibility() const {
    if (const auto &clash_config = App::skipper->config();
        clash_config.external_controller_type == ExternalControllerType::TCPIP) {
        form_layout->setRowVisible(external_controller_edit, true);
        external_controller_edit->setText(QString::fromStdString(_config.external_controller));
        form_layout->setRowVisible(secret_edit, true);
        secret_edit->setText(QString::fromStdString(_config.secret));
        form_layout->setRowVisible(unix_socket_edit, false);
    } else if (clash_config.external_controller_type == ExternalControllerType::UNIX_DOMAIN) {
        form_layout->setRowVisible(external_controller_edit, false);
        form_layout->setRowVisible(secret_edit, false);
        form_layout->setRowVisible(unix_socket_edit, true);
        unix_socket_edit->setText(QString::fromStdString(_config.unix_socket));
    }
}

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
        QString("<strong>Skipper") + APP_VERSION + "<br/>build with Qt" +
        QT_VERSION_STR
        "</strong><br/><a "
        "href='https://github.com/z2z63/hearthstone_skipper'>https://github.com/z2z63/hearthstone_skipper<a/>"));
    setLayout(layout1);
}

AboutTab::~AboutTab() = default;

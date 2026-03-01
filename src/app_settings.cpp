#include "app_settings.h"

AppSettings::AppSettings() : _settings(QSettings()) {
}

// rm ~/Library/Preferences/com.z2z63-dev.skipper.plist
// killall -u $USER cfprefsd
std::optional<ClashConfig> AppSettings::clash_config() const {
    QVariant unix_socket_ = _settings.value("unix_socket");
    QVariant external_controller = _settings.value("external_controller");
    QVariant secret = _settings.value("secret");
    QVariant external_controller_type_ = _settings.value("external_controller_type");
    if (!external_controller_type_.isValid()) {
        return {};
    }
    QString external_controller_type = external_controller_type_.toString();
    if (external_controller_type == "UNIX_DOMAIN") {
        // 使用 unix socket 连接 clash 核心，unix_socket 必填
        if (!unix_socket_.isValid() || unix_socket_.toString().isEmpty()) {
            return {};
        }
        return std::optional(ClashConfig{
            .external_controller_type = ExternalControllerType::UNIX_DOMAIN,
            .external_controller = external_controller.isValid() ? external_controller.toString().toStdString() : "",
            .secret = secret.isValid() ? secret.toString().toStdString() : "",
            .unix_socket = unix_socket_.toString().toStdString(),
        });
    }
    if (external_controller_type_ == "TCPIP") {
        // 使用 TCP 连接 clash 核心，external_controller 必填
        if (external_controller.isValid() || external_controller.toString().isEmpty()) {
            return {};
        }
        return std::optional(ClashConfig{
            .external_controller_type = ExternalControllerType::TCPIP,
            .external_controller = external_controller.toString().toStdString(),
            .secret = secret.isValid() ? secret.toString().toStdString() : "",
            .unix_socket = unix_socket_.isValid() ? unix_socket_.toString().toStdString() : "",
        });
    }
    return {};
}

void AppSettings::clash_config_set(const ClashConfig &value) {
    if (value.external_controller_type == ExternalControllerType::TCPIP) {
        _settings.setValue("external_controller_type", "TCPIP");
    } else if (value.external_controller_type == ExternalControllerType::UNIX_DOMAIN) {
        _settings.setValue("external_controller_type", "UNIX_DOMAIN");
    } else {
        _settings.setValue("external_controller_type", "NONE");
    }
    _settings.setValue("external_controller", QString::fromStdString(value.external_controller));
    _settings.setValue("secret", QString::fromStdString(value.secret));
    _settings.setValue("unix_socket", QString::fromStdString(value.unix_socket));
    _settings.sync();
}

AppSettings &AppSettings::instance() {
    static AppSettings app_settings;
    return app_settings;
}
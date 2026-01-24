#include "app_settings.h"

AppSettings::AppSettings() : _settings(QSettings()) {
}

QString AppSettings::unix_socket() const {
    const QVariant v = _settings.value("unix_socket");
    return v.toString();
}

QString AppSettings::external_controller() const {
    const QVariant v = _settings.value("external_controller");
    return v.toString();
}

QString AppSettings::secret() const {
    const QVariant v = _settings.value("secret");
    return v.toString();
}

void AppSettings::unix_socket_set(const QString &value) {
    _settings.setValue("unix_socket", value);
    _settings.sync();
}

void AppSettings::external_controller_set(const QString &value) {
    _settings.setValue("external_controller", value);
    _settings.sync();
}

void AppSettings::secret_set(const QString &value) {
    _settings.setValue("secret", value);
    _settings.sync();
}


const AppSettings &AppSettings::instance() {
    static AppSettings app_settings;
    return app_settings;
}
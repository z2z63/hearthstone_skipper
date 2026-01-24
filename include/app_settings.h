#pragma once

#include <optional>
#include <QString>
#include <QSettings>

class AppSettings {
public:
    AppSettings();
    [[nodiscard]] QString unix_socket() const;
    [[nodiscard]] QString external_controller() const;
    [[nodiscard]] QString secret() const;
    void unix_socket_set(const QString &value);
    void external_controller_set(const QString &value);
    void secret_set(const QString &value);

    static const AppSettings &instance();

private:
    QSettings _settings;
};
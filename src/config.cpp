#include "config.h"
#include <QSettings>
#include <QDir>
#include <QLocalSocket>
#include "logger.h"
#include "yaml-cpp/yaml.h"

std::unique_ptr<QNetworkAccessManager> manager;

ClashConfig::ClashConfig(const std::string &external_controller, const std::string &secret) :
    external_controller(external_controller), secret(secret), _logger(spdlog::get("skipper")) {
}


ConfigState ClashConfig::tryConfig() {
    const QSettings settings;
    const QString _external_controller = settings.value("external_controller").toString();
    const QString _secret = settings.value("secret").toString();
    if (!_external_controller.isEmpty()) {
        external_controller = _external_controller.toStdString();
        secret = _secret.toStdString();
        return ConfigState::GET_FROM_SETTINGS;
    }
    return deduceFromConfigFile();

}


ConfigState ClashConfig::deduceFromConfigFile() {
    QFile clash_config_file(QDir::homePath() + "/.config/clash/config.yaml");
    if (!clash_config_file.open(QIODevice::ReadOnly)) {
        _logger->warn("Failed to open clash config file: {}", clash_config_file.errorString().toStdString());
        return ConfigState::NONE;
    }
    QString config_string = clash_config_file.readAll();
    clash_config_file.close();
    YAML::Node root_node = YAML::Load(config_string.toStdString());
    auto _external_controller = root_node["external-controller"].as<std::string>();
    auto _secret = root_node["secret"].as<std::string>();
    if (_external_controller.empty()) {
        _logger->warn("No external controller specified in config.yaml");
        return ConfigState::NONE;
    }
    external_controller = _external_controller;
    secret = _secret;
    return ConfigState::CLASH_FOR_WINDOWS_DEDUCED;
}

ConfigState ClashConfig::deduceForClashVerge() {
// #ifdef Q_OS_DARWIN
//     QString unix_socket = "/var/tmp/verge/verge-mihomo.sock";
//     QLocalSocket socket =  QLocalSocket();
//     socket.connectToServer(unix_socket, QIODeviceBase::ReadWrite);
//     QObject::connect(&socket, &QLocalSocket::connected, [&]() {
//         QString http_request = "GET /version HTTP/1.1\r\n"
//     })

//
// #else
//     return ConfigState::NONE;
// #endif
    return ConfigState::NONE;
}

ConfigState ClashConfig::deduceForCFW() {
    return ConfigState::NONE;
}
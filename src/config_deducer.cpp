#include "config_deducer.h"
#include "app_settings.h"
#include "spdlog/spdlog.h"
#include "yaml-cpp/yaml.h"
#include <QJsonObject>
#include <curl/curl.h>

#include <QDir>
#include <QFile>
#include <sys/stat.h>

ConfigDeducer::ConfigDeducer(ConfigAwareQEasy *qeasy, QObject *parent)
    : QObject(parent), _state(ConfigDeduceState::CLASH_VERGE), _qeasy(qeasy), _logger(spdlog::get("skipper")) {
}

void ConfigDeducer::tryDeduce() {
    startNextTest();
}

void ConfigDeducer::deduce_cfw() {
    SPDLOG_LOGGER_INFO(_logger, "start deduce for cfw");
    QFile clash_config_file(QDir::homePath() + "/.config/clash/config.yaml");
    if (!clash_config_file.open(QIODevice::ReadOnly)) {
        _logger->warn("Failed to open clash config file: {}", clash_config_file.errorString().toStdString());
        startNextTest();
        return;
    }
    QString config_string = clash_config_file.readAll();
    clash_config_file.close();
    YAML::Node root_node = YAML::Load(config_string.toStdString());
    std::string _external_controller{}, _secret{};
    if (root_node["external-controller"]) {
        _external_controller = root_node["external-controller"].as<std::string>();
    }
    if (root_node["secret"]) {
        _secret = root_node["secret"].as<std::string>();
    }
    if (_external_controller.empty()) {
        _logger->warn("No external controller specified in config.yaml");
        startNextTest();
        return;
    }
    _qeasy->changeConfig({.external_controller_type = ExternalControllerType::TCPIP,
                          .external_controller = _external_controller,
                          .secret = _secret});
    _qeasy->test();
}

#ifdef Q_OS_MACOS

void ConfigDeducer::deduce_clash_verge() {
    SPDLOG_LOGGER_INFO(_logger, "start deduce for clash verge");
    static const std::string socket_path = "/tmp/verge/verge-mihomo.sock";

    struct stat socket_stat{};
    if (stat(socket_path.c_str(), &socket_stat) < 0) {
        SPDLOG_LOGGER_INFO(_logger, "/tmp/verge/verge-mihomo.sock not exists, errno={}", strerror(errno));
        startNextTest();
        return;
    }
    bool isSocket = S_ISSOCK(socket_stat.st_mode);
    bool canRead = S_IRUSR & socket_stat.st_mode;
    bool canWrite = S_IWUSR & socket_stat.st_mode;
    if (!(isSocket && canRead && canWrite)) {
        SPDLOG_LOGGER_INFO(_logger,
                           "/tmp/verge/verge-mihomo.sock permission denied isSocket={}, canRead={}, canWrite={}",
                           isSocket, canRead, canWrite);
        startNextTest();
        return;
    }
    _qeasy->changeConfig({.external_controller_type = ExternalControllerType::UNIX_DOMAIN,
                          .external_controller = "",
                          .secret = "",
                          .unix_socket = socket_path});
    _qeasy->test();
}

#endif

void ConfigDeducer::startNextTest() {
    switch (_state) {
    case ConfigDeduceState::CLASH_VERGE:
        connect(_qeasy, &ConfigAwareQEasy::testFinished, this, &ConfigDeducer::handleTestFinish);
        _state = ConfigDeduceState::CLASH_FOR_WINDOWS;
        deduce_clash_verge();
        break;
    case ConfigDeduceState::CLASH_FOR_WINDOWS:
        _state = ConfigDeduceState::FINISH;
        deduce_cfw();
        break;
    case ConfigDeduceState::FINISH:
        SPDLOG_LOGGER_INFO(_logger, "config deduce failed");
        disconnect(_qeasy, &ConfigAwareQEasy::testFinished, this, &ConfigDeducer::handleTestFinish);
        emit deduceFinished(nullptr);
        break;
    }
}

void ConfigDeducer::handleTestFinish(const bool testSuccess) {
    if (testSuccess) {
        SPDLOG_LOGGER_INFO(_logger, "config deduce succeed");
        disconnect(_qeasy, &ConfigAwareQEasy::testFinished, this, &ConfigDeducer::handleTestFinish);
        emit deduceFinished(_qeasy);
    } else {
        startNextTest();
    }
}

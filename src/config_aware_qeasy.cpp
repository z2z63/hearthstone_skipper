#include "config_aware_qeasy.h"
#include <QJsonValue>
#include <utility>

ConfigAwareQEasy::ConfigAwareQEasy(ClashConfig config, QObject *parent) : QCurlEasy(curl_easy_init(), parent),
                                                                          _config(std::move(config)),
                                                                          _logger(spdlog::get("skipper")) {
    changeConfig(_config);
}

ConfigAwareQEasy::~ConfigAwareQEasy() = default;

void ConfigAwareQEasy::test() {
    SPDLOG_LOGGER_INFO(_logger, "controller_test begin type={} address={} unix_socket={} secret_set={}",
                       static_cast<int>(_config.external_controller_type), _config.external_controller,
                       _config.unix_socket, !_config.secret.empty());
    if (isRunning()) {
        emit testFinished(false, "skipper正在执行其他网络请求");
        return;
    }
    if (_config.external_controller_type == ExternalControllerType::NONE) {
        emit testFinished(false, "skipper未完成设置");
        return;
    }
    if (_config.external_controller_type == ExternalControllerType::TCPIP && _config.external_controller.empty()) {
        SPDLOG_LOGGER_INFO(_logger, "external controller is empty");
        emit testFinished(false, "skipper未完成设置");
        return;
    }
    if (_config.external_controller_type == ExternalControllerType::UNIX_DOMAIN && _config.unix_socket.empty()) {
        SPDLOG_LOGGER_INFO(_logger, "unix domain is empty");
        emit testFinished(false, "skipper未完成设置");
        return;
    }
    curl_easy_setopt(curl, CURLOPT_URL, _config.version().c_str());
    if (!perform()) {
        emit testFinished(false, "无法启动检测请求");
        return;
    }
    connect(this, &QCurlEasy::done, this, &ConfigAwareQEasy::handle_version_response, Qt::SingleShotConnection);
}

void ConfigAwareQEasy::handle_version_response(const QString &error, long code, const QByteArray &body) {
    SPDLOG_LOGGER_INFO(_logger, "controller_test finish error={} code={} response={}", error.toStdString(), code,
                       body.toStdString());
    if (!error.isEmpty() || code / 100 != 2) {

        emit testFinished(false, fmt::format("/version error={}, code={}", error.toStdString(), code));
        return;
    }
    if (QJsonDocument doc = QJsonDocument::fromJson(body);
        !doc.isObject() || !doc["version"].isString() || doc["version"].toString().isEmpty()) {
        emit testFinished(false, fmt::format("/version body={}", doc["version"].toString().toStdString()));
        return;
    }
    emit testFinished(true, "");
}

void ConfigAwareQEasy::changeConfig(const ClashConfig &newConfig) {
    _config = newConfig;
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, nullptr);
    curl_slist_free_all(headers);
    headers = nullptr;
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
    if (_config.external_controller_type == ExternalControllerType::NONE) {
        return;
    }
    if (_config.external_controller_type == ExternalControllerType::UNIX_DOMAIN && !_config.unix_socket.empty()) {
        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, _config.unix_socket.c_str());
    }
    if (!_config.secret.empty()) {
        const std::string header = "Authorization: Bearer " + _config.secret;
        headers = curl_slist_append(nullptr, header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
}

const ClashConfig &ConfigAwareQEasy::config() const {
    return _config;
}

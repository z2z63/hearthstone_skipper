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
    if (_config.external_controller.empty()) {
        SPDLOG_LOGGER_INFO(_logger, "external controller is empty");
        emit testFinished(false);
        return;
    }
    curl_easy_setopt(curl, CURLOPT_URL, _config.version().c_str());
    connect(this, &QCurlEasy::done, this, &ConfigAwareQEasy::handle_version_response, Qt::SingleShotConnection);
    perform();
}

void ConfigAwareQEasy::handle_version_response(const QString &error, long code, const QByteArray &body) {
    if (!error.isEmpty() || code / 100 != 2) {
        emit testFinished(false);
        return;
    }
    if (QJsonDocument doc = QJsonDocument::fromJson(body);
        !doc.isObject() || !doc["version"].isString() || doc["version"].toString().isEmpty()) {
        emit testFinished(false);
        return;
    }
    emit testFinished(true);
}

void ConfigAwareQEasy::changeConfig(const ClashConfig &newConfig) {
    _config = newConfig;
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, nullptr);
    curl_slist_free_all(headers);
    headers = nullptr;
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
    if (_config.config_deduce_source == ExternalControllerType::NONE) {
        return;
    }
    if (_config.config_deduce_source == ExternalControllerType::UNIX_DOMAIN && !_config.unix_socket.empty()) {
        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, _config.unix_socket.c_str());
    }
    if (!_config.secret.empty()) {
        const std::string header = "Authorization:Bearer " + _config.secret;
        headers = curl_slist_append(nullptr, header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
}

const ClashConfig &ConfigAwareQEasy::config() const {
    return _config;
}
#include <memory>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>

#include "yaml-cpp/yaml.h"

#include "skipper.h"

using namespace std::string_literals;

Skipper::Skipper(ConfigAwareQEasy *qeasy, QObject *parent)
    : QObject(parent), _qeasy(qeasy), _logger(spdlog::get("skipper")) {
    assert(qeasy != nullptr);
    connect(_qeasy, &ConfigAwareQEasy::testFinished, this,
            [this](bool testSuccess, const std::string &message) { emit testFinished(testSuccess, message); });
}

Skipper::~Skipper() = default;

void Skipper::skip() {
    getConnection();
}

void Skipper::test() const {
    _qeasy->test();
}

const ClashConfig &Skipper::config() const {
    return _qeasy->config();
}

void Skipper::setConfig(const ClashConfig &config) const {
    _qeasy->changeConfig(config);
}

void Skipper::getConnection() {
    const std::string url = _qeasy->config().connections();
    curl_easy_setopt(_qeasy->curl, CURLOPT_URL, url.c_str());
    connect(_qeasy, &QCurlEasy::done, this, &Skipper::handleGetConnectionThenKill, Qt::SingleShotConnection);
    _qeasy->perform();
}

void Skipper::handleGetConnectionThenKill(const QString &error, long code, const QByteArray &body) {
    const std::string url = _qeasy->config().connections();
    if (!error.isEmpty()) {
        SPDLOG_LOGGER_ERROR(_logger, "GET {} failed: {}", url, error.toStdString());
        _logger->flush();
        return;
    }
    if (code / 100 != 2) {
        SPDLOG_LOGGER_ERROR(_logger, "GET {} code={} body={}", url, code, body.toStdString());
        _logger->flush();
        return;
    }
    auto doc = QJsonDocument::fromJson(body);
    if (!doc["connections"].isArray()) {
        SPDLOG_LOGGER_WARN(_logger, "Failed to get connection, malformed json {}", body.toStdString());
        _logger->flush();
        return;
    }
    std::string connection_to_kill;
    for (auto obj : doc["connections"].toArray()) {
        if (obj.isObject() &&
            obj.toObject()["metadata"].toObject()["processPath"].toString().endsWith(
                "Hearthstone.app/Contents/MacOS/Hearthstone") &&
            obj.toObject()["metadata"].toObject()["host"] == "") {
            connection_to_kill = obj.toObject()["id"].toString().toStdString();
            SPDLOG_LOGGER_INFO(_logger, "Connection to kill {}", connection_to_kill);
            break;
        }
    }
    if (connection_to_kill.empty()) {
        SPDLOG_LOGGER_WARN(_logger, "No connection to kill");
        _logger->flush();
        return;
    }
    const std::string url2 = _qeasy->config().kill_connection(connection_to_kill);
    curl_easy_setopt(_qeasy->curl, CURLOPT_URL, url2.c_str());
    curl_easy_setopt(_qeasy->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    connect(_qeasy, &QCurlEasy::done, this, &Skipper::handleKillConnection, Qt::SingleShotConnection);
    _qeasy->perform();
}

void Skipper::handleKillConnection(const QString &error, long code, const QByteArray &body) {
    curl_easy_setopt(_qeasy->curl, CURLOPT_CUSTOMREQUEST, nullptr);
    curl_easy_setopt(_qeasy->curl, CURLOPT_HTTPGET, 1L);    // 恢复 CURL 的内部状态
    char *url;
    curl_easy_getinfo(_qeasy->curl, CURLINFO_EFFECTIVE_URL, &url);

    if (!error.isEmpty()) {
        SPDLOG_LOGGER_ERROR(_logger, "DELETE {} failed: {}", url, error.toStdString());
        _logger->flush();
        return;
    }
    if (code / 100 != 2) {
        SPDLOG_LOGGER_ERROR(_logger, "DELETE {} failed code={} body={}", url, code, body.toStdString());
        _logger->flush();
        return;
    }
    SPDLOG_LOGGER_INFO(_logger, "DELETE {}", url);
}
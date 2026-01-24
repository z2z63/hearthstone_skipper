#include <memory>

#include <QFile>
#include <QDir>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <utility>

#include "yaml-cpp/yaml.h"

#include "skipper.h"

using namespace std::string_literals;


Skipper::Skipper(ClashConfig config, QObject* parent) :QObject(parent) , _logger(spdlog::get("skipper")) {
    qeasy = new ConfigAwareQEasy(std::move(config));
    connect(qeasy, &ConfigAwareQEasy::testFinished, this, [this](bool testSuccess) {
        emit testFinished(testSuccess);
    });
}

Skipper::~Skipper() = default;

void Skipper::skip() {
    getConnection();
}

void Skipper::test() const {
    qeasy->test();
}


const ClashConfig &Skipper::config() const {
    return qeasy->config();
}

void Skipper::setExternalController(const std::string &external_controller) {
    ClashConfig config = qeasy->config();
    config.external_controller = external_controller;
    qeasy->changeConfig(config);
}

void Skipper::setSecret(const std::string &secret) const {
    ClashConfig config = qeasy->config();
    config.secret = secret;
    qeasy->changeConfig(config);
}


void Skipper::setConfig(const ClashConfig &config) const {
    qeasy->changeConfig(config);
}

void Skipper::getConnection() {
    const std::string url = qeasy->config().connections();
    curl_easy_setopt(qeasy->curl, CURLOPT_URL, url.c_str());
    connect(qeasy, &QCurlEasy::done, this, &Skipper::handleGetConnectionAndDeleteConnection, Qt::SingleShotConnection);
    qeasy->perform();
}

void Skipper::handleGetConnectionAndDeleteConnection(QString error, long code, QByteArray body) {
    const std::string url = qeasy->config().connections();
    if (!error.isEmpty()) {
        SPDLOG_LOGGER_ERROR(_logger, "GET {} failed: {}", url, error.toStdString());
        _logger->flush();
        return;
    }
    if (code != 200) {
        SPDLOG_LOGGER_ERROR(_logger, "GET {} code={} body={}", url, code, body.toStdString());
        _logger->flush();
        return;
    }
    auto doc = QJsonDocument::fromJson(body);
    //  /Applications/Hearthstone/Hearthstone.app
    if (!doc["connections"].isArray()) {
        SPDLOG_LOGGER_WARN(_logger, "Failed to get connection, malformed json {}",
                           body.toStdString());
        _logger->flush();
        return;
    }
    std::string connection_to_kill;
    for (auto obj : doc["connections"].toArray()) {
        if (obj.isObject() && obj.toObject()["metadata"].toObject()["processPath"] ==
            "/Applications/Hearthstone/Hearthstone.app/Contents/MacOS/Hearthstone" && obj.
            toObject()["metadata"].toObject()["host"] == "") {
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
    const static std::string url2 = qeasy->config().kill_connection(connection_to_kill);
    curl_easy_setopt(qeasy->curl, CURLOPT_URL, url2.c_str());
    curl_easy_setopt(qeasy->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    connect(qeasy, &QCurlEasy::done, this, &Skipper::handleDeleteConnection, Qt::SingleShotConnection);
    qeasy->perform();
}

void Skipper::handleDeleteConnection(const QString &error, long code, const QByteArray &body) {
    char *url, *method;
    curl_easy_getinfo(qeasy->curl, CURLINFO_EFFECTIVE_URL, &url);
    curl_easy_getinfo(qeasy->curl, CURLINFO_REDIRECT_URL, &method);

    if (!error.isEmpty()) {
        SPDLOG_LOGGER_ERROR(_logger, "{} {} failed: {}", method, url, error.toStdString());
        _logger->flush();
        return;
    }
    if (code != 200) {
        SPDLOG_LOGGER_ERROR(_logger, "{} {} failed code={} body={}", method, url, code, body.toStdString());
        _logger->flush();
        return;
    }
}

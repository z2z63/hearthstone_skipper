#include <memory>

#include <QFile>
#include <QDir>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

#include "yaml-cpp/yaml.h"

#include "skipper.h"

using namespace std::string_literals;


Skipper::Skipper() : _config("", ""), _logger(spdlog::get("skipper")) {
}

Skipper::~Skipper() = default;

void Skipper::skip() const {
    auto url = QString::fromStdString(
        "http://"s + _config.external_controller);
    QNetworkRequest request1(url + "/connections");
    request1.setRawHeader("Authorization",
                          QByteArray::fromStdString("Bearer "s + _config.secret));
    QNetworkReply *reply1 = manager->get(request1);
    QObject::connect(reply1, &QNetworkReply::finished,
                     [url,reply1,this] {
                         auto json_string = reply1->readAll();
                         if (reply1->error() != QNetworkReply::NoError) {
                             SPDLOG_LOGGER_WARN(_logger, "Failed to get connection, network error: {}",
                                                reply1->errorString().toStdString());
                             reply1->deleteLater();
                             _logger->flush();
                             return;
                         }
                         if (int code = reply1->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                             code != 200) {
                             SPDLOG_LOGGER_WARN(_logger, "Failed to get connection, http status code: {}", code);
                             reply1->deleteLater();
                             _logger->flush();
                             return;
                         }
                         reply1->deleteLater();
                         QJsonDocument json_document = QJsonDocument::fromJson(
                             json_string);
                         //  /Applications/Hearthstone/Hearthstone.app
                         if (!json_document["connections"].isArray()) {
                             SPDLOG_LOGGER_WARN(_logger, "Failed to get connection, malformed json {}",
                                                json_string.toStdString());
                             _logger->flush();
                             return;
                         }
                         std::string connection_to_kill;
                         for (auto obj : json_document["connections"].toArray()) {
                             if (obj.isObject() && obj.toObject()["metadata"].toObject()["processPath"] ==
                                 "/Applications/Hearthstone/Hearthstone.app/Contents/MacOS/Hearthstone" && obj.
                                 toObject()["metadata"].toObject()["host"] == "") {
                                 connection_to_kill = obj.toObject()["id"].toString().toStdString();
                                 SPDLOG_LOGGER_INFO(_logger, "Connection to kill {}", connection_to_kill);
                             }
                         }
                         if (connection_to_kill.empty()) {
                             SPDLOG_LOGGER_WARN(_logger, "No connection to kill");
                             _logger->flush();
                             return;
                         }

                         QNetworkRequest request2(url + "/connections/" + QString::fromStdString(connection_to_kill));
                         request2.setRawHeader("Authorization",
                                               QByteArray::fromStdString("Bearer "s + _config.secret));
                         QNetworkReply *reply2 = manager->deleteResource(request2);
                         QObject::connect(reply2, &QNetworkReply::finished,
                                          [this, connection_to_kill, reply2] {
                                              if (reply2->error() != QNetworkReply::NoError) {
                                                  SPDLOG_LOGGER_WARN(
                                                      _logger, "kill connection failed, netowkr error: {}",
                                                      reply2->errorString().toStdString());
                                                  reply2->deleteLater();
                                                  _logger->flush();
                                                  return;
                                              }

                                              if (int code = reply2->
                                                             attribute(QNetworkRequest::HttpStatusCodeAttribute).
                                                             toInt(); code != 200) {
                                                  SPDLOG_LOGGER_WARN(
                                                      _logger, "kill connection failed, http status code: {}", code);
                                                  reply2->deleteLater();
                                                  _logger->flush();
                                                  return;
                                              }
                                              reply2->deleteLater();
                                              SPDLOG_LOGGER_INFO(_logger, "connection {} killed", connection_to_kill);
                                              _logger->flush();
                                          });
                     }
        );
}

void Skipper::test() {
    // get /version
    auto url = QString::fromStdString(
        "http://"s + _config.external_controller);
    QNetworkRequest request(url + "/version");
    request.setRawHeader("Authorization",
                         QByteArray::fromStdString("Bearer "s + _config.secret));
    QNetworkReply *reply = manager->get(request);
    connect(reply, &QNetworkReply::finished,
            [reply, this] {
                auto json_string = reply->readAll();
                if (reply->error() != QNetworkReply::NoError) {
                    SPDLOG_LOGGER_WARN(_logger, "Failed to get version, network error: {}",
                                       reply->errorString().toStdString());
                    reply->deleteLater();
                    emit testFinished(false);
                    _logger->flush();
                    return;
                }
                if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                    SPDLOG_LOGGER_WARN(_logger, "Failed to get version, http status code: {}",
                                       reply->errorString().toStdString());
                    reply->deleteLater();
                    emit testFinished(false);
                    _logger->flush();
                    return;
                }
                reply->deleteLater();
                QJsonDocument json_document = QJsonDocument::fromJson(
                    json_string);
                if (!json_document.isObject()) {
                    SPDLOG_LOGGER_WARN(_logger, "Failed to get version, malformed json {}", json_string.toStdString());
                    reply->deleteLater();
                    emit testFinished(false);
                    _logger->flush();
                    return;
                }
                reply->deleteLater();
                SPDLOG_LOGGER_INFO(_logger, "clash version {}", json_document["version"].toString().toStdString());
                emit testFinished(true);
                _logger->flush();
            }
        );

}


const ClashConfig &Skipper::config() const {
    return _config;
}

void Skipper::setExternalController(const std::string &external_controller) {
    _config.external_controller = external_controller;
}

void Skipper::setSecret(const std::string &secret) {
    _config.secret = secret;
}

ConfigState Skipper::tryConfig() {
    return _config.tryConfig();

}

void Skipper::setConfig(const ClashConfig &config) {
    _config = config;
}
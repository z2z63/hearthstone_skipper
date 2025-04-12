#include <memory>

#include <QFile>
#include <QDir>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

#include "yaml-cpp/yaml.h"

#include "logger.h"

#include "skipper.h"

using namespace std::string_literals;


std::unique_ptr<QNetworkAccessManager> Skipper::_manager = std::make_unique<QNetworkAccessManager>();

Skipper::Skipper(): _config({"", ""}) {
}

Skipper::~Skipper() = default;

void Skipper::skip() const {
    auto url = QString::fromStdString(
        "http://"s + _config.external_controller);
    QNetworkRequest request1(url + "/connections");
    request1.setRawHeader("Authorization",
                          QByteArray::fromStdString("Bearer "s + _config.secret));
    QNetworkReply *reply1 = _manager->get(request1);
    QObject::connect(reply1, &QNetworkReply::finished,
                     [url,reply1,this] {
                         auto json_string = reply1->readAll();
                         if (reply1->error() != QNetworkReply::NoError) {
                             logger->warn("Failed to get connection, network error: {}",
                                          reply1->errorString().toStdString());
                             reply1->deleteLater();
                             return;
                         }
                         if (reply1->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                             logger->warn("Failed to get connection, http status code: {}",
                                          reply1->errorString().toStdString());
                             reply1->deleteLater();
                             return;
                         }
                         reply1->deleteLater();
                         QJsonDocument json_document = QJsonDocument::fromJson(
                             json_string);
                         //  /Applications/Hearthstone/Hearthstone.app
                         if (!json_document["connections"].isArray()) {
                             logger->warn("Failed to get connection, malformed json {}", json_string.toStdString());
                             return;
                         }
                         std::string connection_to_kill;
                         for (auto obj : json_document["connections"].toArray()) {
                             if (obj.isObject() && obj.toObject()["metadata"].toObject()["processPath"] ==
                                 "/Applications/Hearthstone/Hearthstone.app/Contents/MacOS/Hearthstone" && obj.
                                 toObject()["metadata"].toObject()["host"] == "") {
                                 connection_to_kill = obj.toObject()["id"].toString().toStdString();
                                 logger->info("Connection to kill {}", connection_to_kill);
                             }
                         }
                         if (connection_to_kill.empty()) {
                             logger->warn("No connection to kill");
                             return;
                         }

                         QNetworkRequest request2(url + "/connections/" + QString::fromStdString(connection_to_kill));
                         request2.setRawHeader("Authorization",
                                               QByteArray::fromStdString("Bearer "s + _config.secret));
                         QNetworkReply *reply2 = _manager->deleteResource(request2);
                         QObject::connect(reply2, &QNetworkReply::finished,
                                          [connection_to_kill, reply2] {
                                              if (reply2->error() != QNetworkReply::NoError) {
                                                  logger->warn("kill connection failed, netowkr error: {}",
                                                               reply2->errorString().toStdString());
                                                  reply2->deleteLater();
                                                  return;
                                              }
                                              if (reply2->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() !=
                                                  200) {
                                                  logger->warn("kill connection failed, http status code: {}",
                                                               reply2->errorString().toStdString());
                                                  reply2->deleteLater();
                                                  return;
                                              }
                                              reply2->deleteLater();
                                              logger->info("connection {} killed", connection_to_kill);
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
    QNetworkReply *reply = _manager->get(request);
    connect(reply, &QNetworkReply::finished,
            [reply, this] {
                auto json_string = reply->readAll();
                if (reply->error() != QNetworkReply::NoError) {
                    logger->warn("Failed to get version, network error: {}",
                                 reply->errorString().toStdString());
                    reply->deleteLater();
                    emit testFinished(false);
                    return;
                }
                if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                    logger->warn("Failed to get version, http status code: {}",
                                 reply->errorString().toStdString());
                    reply->deleteLater();
                    emit testFinished(false);
                    return;
                }
                reply->deleteLater();
                QJsonDocument json_document = QJsonDocument::fromJson(
                    json_string);
                if (!json_document.isObject()) {
                    logger->warn("Failed to get version, malformed json {}", json_string.toStdString());
                    reply->deleteLater();
                    emit testFinished(false);
                    return;
                }
                reply->deleteLater();
                logger->info("clash version {}", json_document["version"].toString().toStdString());
                emit testFinished(true);
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
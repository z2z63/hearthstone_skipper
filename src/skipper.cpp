#include <memory>

#include <QFile>
#include <QDir>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "yaml-cpp/yaml.h"

#include "logger.h"

#include "skipper.h"

using namespace std::string_literals;


Skipper::Skipper(ClashConfig config) : config(std::move(config)), manager(new QNetworkAccessManager) {
}

Skipper::~Skipper() {
    delete manager;
}


std::unique_ptr<Skipper> Skipper::create() {
    QFile clash_config_file(QDir::homePath() + "/.config/clash/config.yaml");
    if (!clash_config_file.open(QIODevice::ReadOnly)) {
        logger->warn("Failed to open clash config file: {}", clash_config_file.errorString().toStdString());
        return nullptr;
    }
    QString config_string = clash_config_file.readAll();
    clash_config_file.close();
    YAML::Node root_node = YAML::Load(config_string.toStdString());
    auto external_controller = root_node["external-controller"].as<std::string>();
    auto secret = root_node["secret"].as<std::string>();
    if (external_controller.empty()) {
        logger->warn("No external controller specified in config.yaml");
        return nullptr;
    }
    return std::unique_ptr<Skipper>(new Skipper{{external_controller, secret}});
}

void Skipper::skip() {
    auto url = QString::fromStdString(
        "http://"s + config.external_controller);
    QNetworkRequest request1(url + "/connections");
    request1.setRawHeader("Authorization",
                          QByteArray::fromStdString("Bearer "s + config.secret));
    QNetworkReply *reply1 = manager->get(request1);
    QObject::connect(reply1, &QNetworkReply::finished,
                     [url,reply1,this] {
                         auto json_string = reply1->readAll();
                         if (reply1->error() != QNetworkReply::NoError) {
                             logger->warn("Failed to get connection, network error: {}", reply1->errorString().toStdString());
                             reply1->deleteLater();
                             return;
                         }
                         if (reply1->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                             logger->warn("Failed to get connection, http status code: {}", reply1->errorString().toStdString());
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
                                               QByteArray::fromStdString("Bearer "s + config.secret));
                         QNetworkReply *reply2 = manager->deleteResource(request2);
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
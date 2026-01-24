#include "config_deducer.h"
#include "yaml-cpp/yaml.h"
#include "spdlog/spdlog.h"
#include <QJsonObject>
#include <curl/curl.h>
#include "app_settings.h"

#ifdef Q_OS_MACOS
#include <sys/stat.h>
#endif

ConfigDeducer::ConfigDeducer(ConfigAwareQEasy *qeasy, QObject *parent) : QObject(parent), _qeasy(qeasy),
                                                                         _logger(spdlog::get("skipper")) {
    connect(_qeasy, &ConfigAwareQEasy::testFinished, this, &ConfigDeducer::handleTestFinish);
}


void ConfigDeducer::tryDeduce() {
    startNextTest();
}


void ConfigDeducer::deduce_cfw() {
    startNextTest();
}

#ifdef Q_OS_MACOS

void ConfigDeducer::deduce_clash_verge() {
    static const std::string socket_path = "/tmp/verge/verge-mihomo.sock";

    struct stat socket_stat{};
    if (stat(socket_path.c_str(), &socket_stat) < 0) {
        startNextTest();
        return;
    }
    if (!(S_ISSOCK(socket_stat.st_mode) && (S_IRUSR & socket_stat.st_mode) && (S_IWUSR & socket_stat.st_mode))) {
        startNextTest();
        return;
    }
    _qeasy->changeConfig({.config_deduce_source = ExternalControllerType::TCPIP,
                           .external_controller = "http://localhost", .secret = "",
                           .unix_socket = socket_path});
    _qeasy->test();
}

#endif

void ConfigDeducer::startNextTest() {
    static ConfigDeduceState state;
    state = ConfigDeduceState::CLASH_VERGE;
    switch (state) {
    case ConfigDeduceState::CLASH_VERGE:
        state = ConfigDeduceState::CLASH_FOR_WINDOWS;
        deduce_clash_verge();
        break;
    case ConfigDeduceState::CLASH_FOR_WINDOWS:
        state = ConfigDeduceState::FINISH;
        deduce_cfw();
        break;
    case ConfigDeduceState::FINISH: emit deduceFinished(nullptr);
        break;
    }
}


void ConfigDeducer::handleTestFinish(const bool testSuccess) {
    if (testSuccess) {
        emit deduceFinished(_qeasy);
    } else {
        startNextTest();
    }

}
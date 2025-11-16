#pragma once

#include <QNetworkAccessManager>
#include <string>
#include <spdlog/spdlog.h>

enum class ConfigState {
    CLASH_FOR_WINDOWS_DEDUCED,
    CLASH_VERGE_DEDUCED,
    GET_FROM_SETTINGS,
    NONE,
};

extern std::unique_ptr<QNetworkAccessManager> manager;

class ClashConfig {
public:
    ClashConfig(const std::string& external_controller="", const std::string& secret="");
    ConfigState tryConfig();

private:
    ConfigState deduceFromConfigFile();

    ConfigState deduceForClashVerge();

    ConfigState deduceForCFW();

public:
    std::string external_controller;
    std::string secret;

private:
    std::shared_ptr<spdlog::logger> _logger;
};
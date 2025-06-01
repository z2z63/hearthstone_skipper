#pragma once

#include <string>
#include <spdlog/spdlog.h>

enum class ConfigState {
    DEDUCED_FROM_FILE,
    GET_FROM_SETTINGS,
    NONE,
};

class ClashConfig {
public:
    ClashConfig(const std::string& external_controller="", const std::string& secret="");
    ConfigState tryConfig();

private:
    ConfigState deduceFromConfigFile();

public:
    std::string external_controller;
    std::string secret;

private:
    std::shared_ptr<spdlog::logger> _logger;
};
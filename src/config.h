#pragma once

#include <string>

enum class ConfigState {
    DEDUCED_FROM_FILE,
    GET_FROM_SETTINGS,
    NONE,
};

class ClashConfig {
public:
    ConfigState tryConfig();

public:
    std::string external_controller;
    std::string secret;

private:
    ConfigState deduceFromConfigFile();
};
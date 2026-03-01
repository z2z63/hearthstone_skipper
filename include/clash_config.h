#pragma once

#include <string>

enum class ExternalControllerType {
    TCPIP,
    UNIX_DOMAIN,
    NONE
};

class ClashConfig {
public:
    ExternalControllerType external_controller_type = ExternalControllerType::NONE;
    std::string external_controller;
    std::string secret;
    std::string unix_socket;

public:
    [[nodiscard]] std::string version() const;
    [[nodiscard]] std::string connections() const;
    [[nodiscard]] std::string kill_connection(const std::string& conn) const;
};
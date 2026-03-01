#include "clash_config.h"

constexpr std::string HTTP = "http://";

std::string ClashConfig::version() const {
    if (external_controller_type == ExternalControllerType::TCPIP) {
        return HTTP + external_controller + "/version";
    }
    return "http://localhost/version";
}

std::string ClashConfig::connections() const {
    if (external_controller_type == ExternalControllerType::TCPIP) {
        return HTTP + external_controller + "/connections";
    }
    return "http://localhost/connections";
}
std::string ClashConfig::kill_connection(const std::string& conn) const {
    if (external_controller_type == ExternalControllerType::TCPIP) {
        return HTTP + external_controller + "/connections/" + conn;
    }
    return "http://localhost/connections/" + conn;
}

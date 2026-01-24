#include "clash_config.h"

constexpr std::string HTTP = "http://";

std::string ClashConfig::version() const {
    return HTTP + external_controller + "/version";
}

std::string ClashConfig::connections() const {
    return HTTP + external_controller + "/connections";
}
std::string ClashConfig::kill_connection(const std::string& conn) const {
    return HTTP + external_controller + "/connections/" + conn;
}

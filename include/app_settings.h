#pragma once

#include "clash_config.h"

#include <QSettings>
#include <QString>
#include <optional>

class AppSettings {
public:
    AppSettings();
    [[nodiscard]] std::optional<ClashConfig> clash_config() const;
    void clash_config_set(const ClashConfig &value);

    static AppSettings &instance();

private:
    QSettings _settings;
};
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
    [[nodiscard]] bool float_button_enabled() const;
    void float_button_enabled_set(bool enabled);

    static AppSettings &instance();

private:
    QSettings _settings;
};
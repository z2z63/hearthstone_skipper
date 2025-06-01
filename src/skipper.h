#pragma once
#include <string>

class QNetworkAccessManager;
#include <QObject>
#include "config.h"

class Skipper : public QObject {
    Q_OBJECT

public:
    Skipper();

    ConfigState tryConfig();

    ~Skipper() override;
    const ClashConfig &config() const;
    void setExternalController(const std::string &external_controller);
    void setSecret(const std::string &secret);
    void setConfig(const ClashConfig &config);
    void skip() const;
    void test();
signals:
    void testFinished(bool);

private:
    static std::unique_ptr<QNetworkAccessManager> _manager;

    ClashConfig _config;

    std::shared_ptr<spdlog::logger> _logger;
};
#pragma once

#include "qcurl.h"
#include "clash_config.h"
#include "spdlog/spdlog.h"

class ConfigAwareQEasy : public QCurlEasy {
    Q_OBJECT

public:
    explicit ConfigAwareQEasy(ClashConfig config, QObject *parent = nullptr);
    ~ConfigAwareQEasy() override;
    void changeConfig(const ClashConfig &newConfig);
    void test();
    [[nodiscard]] const ClashConfig& config() const;
public slots:
    void handle_version_response(const QString& error, long code, const QByteArray& body);
signals:
    void testFinished(bool success);


private:
    ClashConfig _config{};
    std::shared_ptr<spdlog::logger> _logger;
};
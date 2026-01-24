#pragma once
#include <string>
#include "spdlog/spdlog.h"
#include "config_deducer.h"
#include <QObject>

class Skipper : public QObject {
    Q_OBJECT

public:
    explicit Skipper(ClashConfig config, QObject *parent = nullptr);

    ~Skipper() override;
    [[nodiscard]] const ClashConfig &config() const;
    void setExternalController(const std::string &external_controller);
    void setSecret(const std::string &secret) const;
    void setConfig(const ClashConfig &config) const;
    void skip();
    void test() const;
    void getConnection();
    void handleGetConnectionAndDeleteConnection(QString error, long code, QByteArray body);
    void handleDeleteConnection(const QString& error, long code, const QByteArray& body);
signals:
    void testFinished(bool);




private:
    ConfigAwareQEasy* qeasy;
    std::shared_ptr<spdlog::logger> _logger;
};
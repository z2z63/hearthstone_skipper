#pragma once
#include "config_deducer.h"
#include "spdlog/spdlog.h"
#include <QObject>
#include <string>

class Skipper : public QObject {
    Q_OBJECT

  public:
    explicit Skipper(ConfigAwareQEasy *qeasy, QObject *parent = nullptr);

    ~Skipper() override;
    [[nodiscard]] const ClashConfig &config() const;
    void setConfig(const ClashConfig &config) const;
    void skip();
    void test() const;

  signals:
    void testFinished(bool success, std::string message);
    void skipFinished(bool success);

  private:
    void getConnection();
    void handleGetConnectionThenKill(const QString &error, long code, const QByteArray &body);
    void handleKillConnection(const QString &error, long code, const QByteArray &body);

  private:
    ConfigAwareQEasy *_qeasy;
    std::shared_ptr<spdlog::logger> _logger;
};
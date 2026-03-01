#pragma once

#include "config_aware_qeasy.h"
#include "qcurl.h"
#include "spdlog/spdlog.h"

enum class ConfigDeduceState { CLASH_VERGE, CLASH_FOR_WINDOWS, FINISH };

class ConfigDeducer final : public QObject {
    Q_OBJECT

  public:
    ConfigDeducer(ConfigAwareQEasy *qeasy, QObject *parent);
    void tryDeduce();
  signals:

    void deduceFinished(ConfigAwareQEasy *qeasy);

  public:
    void startNextTest();
    void handleTestFinish(bool testSuccess);
    void deduce_cfw();

#ifdef Q_OS_MACOS
    void deduce_clash_verge();
#endif

  private:
    ConfigDeduceState _state;
    ConfigAwareQEasy *_qeasy;
    std::shared_ptr<spdlog::logger> _logger;
};
#pragma once


#include "qcurl.h"
#include "config_aware_qeasy.h"
#include "spdlog/spdlog.h"


enum class ConfigDeduceState {
    CLASH_VERGE,
    CLASH_FOR_WINDOWS,
    FINISH
};

class ConfigDeducer final : public QObject {
Q_OBJECT

    ConfigDeducer(ConfigAwareQEasy* qeasy, QObject* parent);
    void tryDeduce();
signals:

    void deduceFinished(ConfigAwareQEasy* qeasy);
public:
    void startNextTest();
    void handleTestFinish(bool testSuccess);
    void deduce_cfw();



#ifdef Q_OS_MACOS
    void deduce_clash_verge();
#endif


private:
    ConfigAwareQEasy* _qeasy;
    std::shared_ptr<spdlog::logger> _logger;
};
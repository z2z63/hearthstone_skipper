#include "logger.h"

#include <QStandardPaths>
#include <QDir>
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/dist_sink.h"


static QString initLogFilePath() {
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(logDir);
    if (!dir.exists()) {
        if (bool success = dir.mkpath("."); !success) {
            qWarning("Failed to create log directory: %s", qPrintable(logDir));
            exit(1);
        }
    }
    return dir.filePath("log.txt");
}

QString getLogFilePath() {
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(logDir);
    return dir.filePath("log.txt");
}


void initLogger() {
    // 输出到日志文件
    auto regular_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        initLogFilePath().toStdString(),
        1024 * 4,
        1
        );
#ifdef SKIPPER_DEBUG
    // debug 时彩色的终端日志
    auto colorful_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    dist_sink->add_sink(regular_sink);
    dist_sink->add_sink(colorful_sink);
    auto logger = std::make_shared<spdlog::logger>("skipper", dist_sink);

#else
    auto logger = std::make_shared<spdlog::logger>("skipper", regular_sink);
#endif
    spdlog::register_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%@] %v");
}
//
// Created by 张三 on 2/2/25.
//

#include "logger.h"

#include <QDir>
#include "spdlog/sinks/rotating_file_sink.h"

std::shared_ptr<spdlog::logger> logger = spdlog::rotating_logger_mt("hearthstone_skipper",
                                                                    (QDir::homePath() + "/.hearthstone_skipper/log.txt")
                                                                    .toStdString(), 1024 * 4,
                                                                    1);
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cstdio>
#include <thread>

#ifdef Q_OS_MACOS
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#endif

#include "connection_selector.h"
#include "local_connection.h"
#include "skipper.h"

namespace {

const char *controllerTypeName(const ExternalControllerType type) {
    switch (type) {
    case ExternalControllerType::TCPIP:
        return "tcp";
    case ExternalControllerType::UNIX_DOMAIN:
        return "unix";
    case ExternalControllerType::NATIVE:
        return "native-pf";
    case ExternalControllerType::NONE:
        return "none";
    }
    return "unknown";
}

int jsonPort(const QJsonValue &value) {
    bool ok = false;
    const int port = value.isString() ? value.toString().toInt(&ok) : value.toInt(-1);
    return value.isString() && !ok ? -1 : port;
}

void logConnectionScan(const std::shared_ptr<spdlog::logger> &logger, const quint64 skipId,
                       const QJsonArray &connections) {
    int directTcp = 0;
    int knownGamePort = 0;
    int hearthstoneOwned = 0;
    int missingProcess = 0;

    for (qsizetype index = 0; index < connections.size(); ++index) {
        const QJsonObject connection = connections.at(index).toObject();
        const QJsonObject metadata = connection.value("metadata").toObject();
        const QString network = metadata.value("network").toString().toLower();
        const QString host = metadata.value("host").toString();
        const QString processPath = metadata.value("processPath").toString();
        const QString process = metadata.value("process").toString();
        const QString owner = processPath.isEmpty() ? process : processPath;
        const int port = jsonPort(metadata.value("destinationPort"));
        const bool isDirectTcp = host.isEmpty() && (network.isEmpty() || network == "tcp");
        const bool isGamePort = port == 1119 || port == 3724;
        const bool isHearthstone = owner.contains("hearthstone", Qt::CaseInsensitive);

        directTcp += isDirectTcp ? 1 : 0;
        knownGamePort += isGamePort ? 1 : 0;
        hearthstoneOwned += isHearthstone ? 1 : 0;
        missingProcess += owner.isEmpty() ? 1 : 0;

        // Detailed rows are limited to relevant connections. This is enough
        // to diagnose missing process attribution without dumping unrelated
        // browsing destinations into a user-shared log.
        if (isHearthstone || (isDirectTcp && (isGamePort || owner.isEmpty()))) {
            SPDLOG_LOGGER_INFO(
                logger,
                "skip_id={} scan_row={} id={} owner={} network={} direct_ip={} source={}:{} destination={}:{} upload={} download={}",
                skipId, index, connection.value("id").toString().toStdString(), owner.toStdString(),
                network.toStdString(), host.isEmpty(), metadata.value("sourceIP").toString().toStdString(),
                jsonPort(metadata.value("sourcePort")), metadata.value("destinationIP").toString().toStdString(), port,
                connection.value("upload").toDouble(0), connection.value("download").toDouble(0));
        }
    }

    SPDLOG_LOGGER_INFO(logger,
                       "skip_id={} scan_summary total={} direct_tcp={} game_port={} hearthstone_owner={} "
                       "missing_process={}",
                       skipId, connections.size(), directTcp, knownGamePort, hearthstoneOwned, missingProcess);
}

} // namespace

Skipper::Skipper(ConfigAwareQEasy *qeasy, QObject *parent)
    : QObject(parent), _qeasy(qeasy), _logger(spdlog::get("skipper")) {
    assert(qeasy != nullptr);
    connect(_qeasy, &ConfigAwareQEasy::testFinished, this,
            [this](bool testSuccess, const std::string &message) { emit testFinished(testSuccess, message); });
}

Skipper::~Skipper() {
#ifdef Q_OS_MACOS
    if (_nativeAuthorization != nullptr) {
        AuthorizationFree(static_cast<AuthorizationRef>(_nativeAuthorization), kAuthorizationFlagDefaults);
        _nativeAuthorization = nullptr;
    }
#endif
}

void Skipper::skip() {
    if (_busy) {
        SPDLOG_LOGGER_INFO(_logger, "skip_id={} duplicate click while busy, ignoring", _skipId);
        return;
    }
    _busy = true;
    _getAttempts = 0;
    ++_skipId;
    _skipTimer.start();
    const ClashConfig &currentConfig = _qeasy->config();
    SPDLOG_LOGGER_INFO(_logger, "skip_id={} begin controller_type={} controller={} unix_socket={}", _skipId,
                       controllerTypeName(currentConfig.external_controller_type), currentConfig.external_controller,
                       currentConfig.unix_socket);
    _logger->flush();
    if (currentConfig.external_controller_type == ExternalControllerType::NATIVE) {
        nativeDisconnect();
        return;
    }
    getConnection();
}

void Skipper::test() {
    if (_qeasy->config().external_controller_type == ExternalControllerType::NATIVE) {
#ifdef Q_OS_MACOS
        const QFileInfo helper(nativeHelperPath());
        const bool available = helper.isFile() && helper.isExecutable();
        SPDLOG_LOGGER_INFO(_logger, "native_backend_test helper={} available={}",
                           helper.absoluteFilePath().toStdString(), available);
        emit testFinished(available, available ? "" : "原生辅助程序不存在或不可执行");
#else
        emit testFinished(false, "原生 PF 模式仅支持 macOS");
#endif
        return;
    }
    _qeasy->test();
}

const ClashConfig &Skipper::config() const {
    return _qeasy->config();
}

void Skipper::setConfig(const ClashConfig &config) const {
    _qeasy->changeConfig(config);
}

void Skipper::getConnection() {
    ++_getAttempts;
    const std::string url = _qeasy->config().connections();
    SPDLOG_LOGGER_INFO(_logger, "skip_id={} scan attempt={}", _skipId, _getAttempts);
    curl_easy_setopt(_qeasy->curl, CURLOPT_CUSTOMREQUEST, nullptr);
    curl_easy_setopt(_qeasy->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(_qeasy->curl, CURLOPT_URL, url.c_str());
    if (!_qeasy->perform()) {
        SPDLOG_LOGGER_ERROR(_logger, "skip_id={} unable to start GET {}: HTTP client is already busy", _skipId,
                            url);
        finishSkip(false);
        return;
    }
    connect(_qeasy, &QCurlEasy::done, this, &Skipper::handleGetConnectionThenKill, Qt::SingleShotConnection);
}

void Skipper::handleGetConnectionThenKill(const QString &error, long code, const QByteArray &body) {
    const std::string url = _qeasy->config().connections();
    if (!error.isEmpty()) {
        SPDLOG_LOGGER_ERROR(_logger, "skip_id={} GET {} failed: {}", _skipId, url, error.toStdString());
        _logger->flush();
        finishSkip(false);
        return;
    }
    if (code / 100 != 2) {
        SPDLOG_LOGGER_ERROR(_logger, "skip_id={} GET {} code={} body={}", _skipId, url, code,
                            body.toStdString());
        _logger->flush();
        finishSkip(false);
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        SPDLOG_LOGGER_WARN(_logger, "skip_id={} malformed /connections response: error={} body={}", _skipId,
                           parseError.errorString().toStdString(), body.toStdString());
        _logger->flush();
        finishSkip(false);
        return;
    }

    const QJsonValue connections = doc.object().value("connections");
    if (connections.isNull() || connections.isUndefined()) {
        SPDLOG_LOGGER_INFO(_logger, "skip_id={} scan_result connections=null", _skipId);
        retryGetConnection("controller currently reports no proxied connections");
        return;
    }
    if (!connections.isArray()) {
        SPDLOG_LOGGER_WARN(_logger,
                           "skip_id={} malformed /connections response: 'connections' is not an array body={}",
                           _skipId, body.toStdString());
        finishSkip(false);
        return;
    }

    const QJsonArray connectionArray = connections.toArray();
    logConnectionScan(_logger, _skipId, connectionArray);
    const QList<LocalTcpConnection> localConnections = hearthstoneTcpConnections();
    for (const LocalTcpConnection &local : localConnections) {
        if (local.destinationPort == 1119 || local.destinationPort == 3724) {
            SPDLOG_LOGGER_INFO(_logger, "skip_id={} local_hearthstone_socket process={} source={}:{} destination={}:{}",
                               _skipId, local.process.toStdString(), local.sourceIp.toStdString(), local.sourcePort,
                               local.destinationIp.toStdString(), local.destinationPort);
        }
    }
    SPDLOG_LOGGER_INFO(_logger, "skip_id={} local_hearthstone_socket_count={}", _skipId, localConnections.size());
    const std::optional<TcpDestination> gameServer = hearthstoneGameServerDestination();
    if (gameServer.has_value()) {
        SPDLOG_LOGGER_INFO(_logger, "skip_id={} hearthstone_game_log_destination={}:{}", _skipId,
                           gameServer->ip.toStdString(), gameServer->port);
    } else {
        SPDLOG_LOGGER_WARN(_logger, "skip_id={} hearthstone_game_log_destination=unavailable", _skipId);
    }
    const auto selected = selectHearthstoneConnection(connectionArray, localConnections, gameServer);
    if (!selected.has_value()) {
        retryGetConnection("no Hearthstone game-server connection found");
        return;
    }
    SPDLOG_LOGGER_INFO(_logger,
                       "skip_id={} selected id={} process={} source={}:{} destination={}:{} traffic={} score={}", _skipId,
                       selected->id.toStdString(), selected->process.toStdString(),
                       selected->sourceIp.toStdString(), selected->sourcePort, selected->destinationIp.toStdString(),
                       selected->destinationPort, selected->traffic,
                       selected->score);
    const std::string url2 = _qeasy->config().kill_connection(selected->id.toStdString());
    curl_easy_setopt(_qeasy->curl, CURLOPT_URL, url2.c_str());
    curl_easy_setopt(_qeasy->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    if (!_qeasy->perform()) {
        SPDLOG_LOGGER_ERROR(_logger, "skip_id={} unable to start DELETE {}: HTTP client is already busy", _skipId,
                            url2);
        finishSkip(false);
        return;
    }
    connect(_qeasy, &QCurlEasy::done, this, &Skipper::handleKillConnection, Qt::SingleShotConnection);
}

void Skipper::handleKillConnection(const QString &error, long code, const QByteArray &body) {
    curl_easy_setopt(_qeasy->curl, CURLOPT_CUSTOMREQUEST, nullptr);
    curl_easy_setopt(_qeasy->curl, CURLOPT_HTTPGET, 1L);    // 恢复 CURL 的内部状态
    char *url;
    curl_easy_getinfo(_qeasy->curl, CURLINFO_EFFECTIVE_URL, &url);

    if (!error.isEmpty()) {
        SPDLOG_LOGGER_ERROR(_logger, "skip_id={} DELETE {} failed: {}", _skipId, url, error.toStdString());
        _logger->flush();
        finishSkip(false);
        return;
    }
    // A connection disappearing between GET and DELETE is equivalent to the
    // desired outcome: the client no longer owns that session socket.
    if (code == 404 || code == 410) {
        SPDLOG_LOGGER_INFO(_logger, "skip_id={} DELETE {} raced with connection close (code={})", _skipId, url,
                           code);
        finishSkip(true);
        return;
    }
    if (code / 100 != 2) {
        SPDLOG_LOGGER_ERROR(_logger, "skip_id={} DELETE {} failed code={} body={}", _skipId, url, code,
                            body.toStdString());
        _logger->flush();
        finishSkip(false);
        return;
    }
    SPDLOG_LOGGER_INFO(_logger, "skip_id={} DELETE {} code={}", _skipId, url, code);
    finishSkip(true);
}

void Skipper::retryGetConnection(const std::string &reason) {
    constexpr int maxAttempts = 5;
    constexpr int retryDelayMs = 200;
    if (_getAttempts >= maxAttempts) {
        SPDLOG_LOGGER_WARN(_logger, "skip_id={} {} after {} attempts", _skipId, reason, _getAttempts);
        _logger->flush();
        finishSkip(false);
        return;
    }
    SPDLOG_LOGGER_INFO(_logger, "skip_id={} {}; retrying ({}/{})", _skipId, reason, _getAttempts, maxAttempts);
    QTimer::singleShot(retryDelayMs, this, [this] {
        if (_busy) {
            getConnection();
        }
    });
}

void Skipper::finishSkip(const bool success) {
    const qint64 elapsedMs = _skipTimer.isValid() ? _skipTimer.elapsed() : -1;
    SPDLOG_LOGGER_INFO(_logger, "skip_id={} finish success={} attempts={} elapsed_ms={}", _skipId, success,
                       _getAttempts, elapsedMs);
    _logger->flush();
    _busy = false;
    emit skipFinished(success);
}

QString Skipper::nativeHelperPath() const {
    return QCoreApplication::applicationDirPath() + "/../Helpers/skipper-native-helper";
}

void Skipper::nativeDisconnect() {
#ifndef Q_OS_MACOS
    SPDLOG_LOGGER_ERROR(_logger, "skip_id={} native PF backend is only available on macOS", _skipId);
    finishSkip(false);
#else
    const std::optional<TcpDestination> gameServer = hearthstoneGameServerDestination();
    if (!gameServer.has_value()) {
        SPDLOG_LOGGER_WARN(_logger, "skip_id={} native game server unavailable in newest Hearthstone log", _skipId);
        finishSkip(false);
        return;
    }
    const QList<LocalTcpConnection> sockets = hearthstoneTcpConnections();
    const auto socket = std::find_if(sockets.cbegin(), sockets.cend(), [&gameServer](const LocalTcpConnection &item) {
        return item.destinationIp == gameServer->ip && item.destinationPort == gameServer->port;
    });
    if (socket == sockets.cend()) {
        SPDLOG_LOGGER_WARN(_logger, "skip_id={} native game server {}:{} is not an established Hearthstone socket",
                           _skipId, gameServer->ip.toStdString(), gameServer->port);
        finishSkip(false);
        return;
    }
    // 198.18.0.0/15 and Clash Verge's fdfe:dcba:9876::/48 are synthetic
    // TUN addresses. PF cannot reliably target the game's real transport
    // while another tunnel/proxy owns it, so native mode must fail closed.
    const bool tunSource = socket->sourceIp.startsWith("198.18.") ||
                           socket->sourceIp.startsWith("198.19.") ||
                           socket->sourceIp.startsWith("fdfe:dcba:9876:", Qt::CaseInsensitive);
    if (tunSource) {
        SPDLOG_LOGGER_WARN(_logger,
                           "skip_id={} native backend refused synthetic TUN source={}; disable Clash/VPN TUN first",
                           _skipId, socket->sourceIp.toStdString());
        finishSkip(false);
        return;
    }

    const QString helperPath = nativeHelperPath();
    const QFileInfo helper(helperPath);
    if (!helper.isFile() || !helper.isExecutable()) {
        SPDLOG_LOGGER_ERROR(_logger, "skip_id={} native helper unavailable path={}", _skipId,
                            helperPath.toStdString());
        finishSkip(false);
        return;
    }

    SPDLOG_LOGGER_INFO(_logger,
                       "skip_id={} native selected process={} source={}:{} destination={}:{} helper={}", _skipId,
                       socket->process.toStdString(), socket->sourceIp.toStdString(), socket->sourcePort,
                       socket->destinationIp.toStdString(), socket->destinationPort, helperPath.toStdString());
    AuthorizationRef authorization = static_cast<AuthorizationRef>(_nativeAuthorization);
    if (authorization == nullptr) {
        const OSStatus createStatus = AuthorizationCreate(nullptr, kAuthorizationEmptyEnvironment,
                                                          kAuthorizationFlagDefaults, &authorization);
        if (createStatus != errAuthorizationSuccess) {
            SPDLOG_LOGGER_ERROR(_logger, "skip_id={} AuthorizationCreate failed status={}", _skipId, createStatus);
            finishSkip(false);
            return;
        }
        _nativeAuthorization = authorization;
    }

    AuthorizationItem executeItem{kAuthorizationRightExecute, 0, nullptr, 0};
    AuthorizationRights rights{1, &executeItem};
    const AuthorizationFlags flags = kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights |
                                     kAuthorizationFlagPreAuthorize;
    const OSStatus rightsStatus = AuthorizationCopyRights(authorization, &rights, kAuthorizationEmptyEnvironment,
                                                          flags, nullptr);
    if (rightsStatus != errAuthorizationSuccess) {
        SPDLOG_LOGGER_WARN(_logger, "skip_id={} native authorization denied status={}", _skipId, rightsStatus);
        finishSkip(false);
        return;
    }

    const QByteArray helperUtf8 = QFile::encodeName(helperPath);
    const QByteArray addressUtf8 = gameServer->ip.toUtf8();
    const QByteArray portUtf8 = QByteArray::number(gameServer->port);
    QByteArray durationUtf8("1500");
    std::array<char *, 5> arguments{
        const_cast<char *>("disconnect"), const_cast<char *>(addressUtf8.constData()),
        const_cast<char *>(portUtf8.constData()), durationUtf8.data(), nullptr,
    };
    FILE *communicationsPipe = nullptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    const OSStatus executeStatus = AuthorizationExecuteWithPrivileges(
        authorization, helperUtf8.constData(), kAuthorizationFlagDefaults, arguments.data(), &communicationsPipe);
#pragma clang diagnostic pop
    if (executeStatus != errAuthorizationSuccess || communicationsPipe == nullptr) {
        SPDLOG_LOGGER_ERROR(_logger, "skip_id={} native helper launch failed status={}", _skipId, executeStatus);
        finishSkip(false);
        return;
    }

    const quint64 operationSkipId = _skipId;
    QPointer<Skipper> guard(this);
    std::thread([guard, communicationsPipe, operationSkipId] {
        QByteArray output;
        std::array<char, 1024> buffer{};
        size_t count = 0;
        while ((count = std::fread(buffer.data(), 1, buffer.size(), communicationsPipe)) > 0) {
            output.append(buffer.data(), static_cast<qsizetype>(count));
        }
        std::fclose(communicationsPipe);
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, output, operationSkipId] {
                if (guard.isNull()) {
                    return;
                }
                const bool success = output.contains("native disconnect completed");
                SPDLOG_LOGGER_INFO(guard->_logger, "skip_id={} native helper finish success={} output={}",
                                   operationSkipId, success, output.toStdString());
                if (guard->_busy && guard->_skipId == operationSkipId) {
                    guard->finishSkip(success);
                }
            },
            Qt::QueuedConnection);
    }).detach();
#endif
}

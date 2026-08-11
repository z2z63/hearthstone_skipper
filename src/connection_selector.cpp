#include "connection_selector.h"

#include <QJsonObject>
#include <utility>

namespace {

bool isHearthstoneProcess(const QJsonObject &metadata, QString *displayName) {
    QString path = metadata.value("processPath").toString();
    path.replace('\\', '/');
    const QString normalizedPath = path.toLower();
    const QString process = metadata.value("process").toString();
    const QString normalizedProcess = process.toLower();

    const bool pathMatches =
        normalizedPath.endsWith("/hearthstone.app/contents/macos/hearthstone") ||
        normalizedPath.endsWith("/hearthstone.exe");
    const bool nameMatches = normalizedProcess == "hearthstone" || normalizedProcess == "hearthstone.exe";

    if (displayName != nullptr) {
        *displayName = path.isEmpty() ? process : path;
    }
    return pathMatches || nameMatches;
}

quint16 portValue(const QJsonValue &value) {
    bool ok = false;
    const int port = value.isString() ? value.toString().toInt(&ok) : value.toInt(-1);
    if (!value.isString()) {
        ok = port >= 0;
    }
    return ok && port > 0 && port <= 65535 ? static_cast<quint16>(port) : 0;
}

const LocalTcpConnection *matchingLocalConnection(const QJsonObject &metadata,
                                                   const QList<LocalTcpConnection> &localConnections) {
    const QString sourceIp = metadata.value("sourceIP").toString();
    const quint16 sourcePort = portValue(metadata.value("sourcePort"));
    const QString destinationIp = metadata.value("destinationIP").toString();
    const quint16 destinationPort = portValue(metadata.value("destinationPort"));
    if (sourcePort == 0 || destinationPort == 0 || destinationIp.isEmpty()) {
        return nullptr;
    }

    for (const LocalTcpConnection &local : localConnections) {
        const bool sourceIpMatches = sourceIp.isEmpty() || local.sourceIp.isEmpty() || sourceIp == local.sourceIp;
        if (sourceIpMatches && sourcePort == local.sourcePort && destinationIp == local.destinationIp &&
            destinationPort == local.destinationPort) {
            return &local;
        }
    }
    return nullptr;
}

quint64 nonNegativeInteger(const QJsonValue &value) {
    const double number = value.toDouble(0);
    return number > 0 ? static_cast<quint64>(number) : 0;
}

} // namespace

std::optional<HearthstoneConnection> selectHearthstoneConnection(const QJsonArray &connections) {
    return selectHearthstoneConnection(connections, {});
}

std::optional<HearthstoneConnection>
selectHearthstoneConnection(const QJsonArray &connections, const QList<LocalTcpConnection> &localConnections) {
    return selectHearthstoneConnection(connections, localConnections, std::nullopt);
}

std::optional<HearthstoneConnection>
selectHearthstoneConnection(const QJsonArray &connections, const QList<LocalTcpConnection> &localConnections,
                            const std::optional<TcpDestination> &gameServer) {
    std::optional<HearthstoneConnection> best;

    for (const QJsonValue &value : connections) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject connection = value.toObject();
        const QString id = connection.value("id").toString();
        const QJsonObject metadata = connection.value("metadata").toObject();
        QString process;

        const bool clashOwnerMatches = isHearthstoneProcess(metadata, &process);
        const LocalTcpConnection *localMatch = matchingLocalConnection(metadata, localConnections);
        if (id.isEmpty() || (!clashOwnerMatches && localMatch == nullptr)) {
            continue;
        }
        if (!clashOwnerMatches) {
            process = localMatch->process;
        }
        const QString network = metadata.value("network").toString().toLower();
        if (!network.isEmpty() && network != "tcp") {
            continue;
        }
        // A non-empty host represents a domain connection. The game session
        // itself is connected by IP; killing domain services can exit the
        // client instead of triggering a game reconnect.
        if (!metadata.value("host").toString().isEmpty()) {
            continue;
        }

        const QString destinationIp = metadata.value("destinationIP").toString();
        const QString sourceIp = metadata.value("sourceIP").toString();
        const quint16 sourcePort = portValue(metadata.value("sourcePort"));
        const quint16 port = portValue(metadata.value("destinationPort"));
        const bool exactGameServer = gameServer.has_value() && destinationIp == gameServer->ip &&
                                     port == gameServer->port;
        // When Hearthstone has declared its active game server, refusing all
        // other sockets is safer than guessing. In particular, port 1119 is
        // also used by Battle.net login/services and disconnecting it can
        // produce a fatal login failure during reconnect.
        if (gameServer.has_value() && !exactGameServer) {
            continue;
        }
        const int portPreference = port == 3724 ? 200 : (port == 1119 ? 100 : 0);
        const int score = (localMatch != nullptr ? 300 : 100) + portPreference + (exactGameServer ? 1000 : 0);
        const quint64 traffic = nonNegativeInteger(connection.value("upload")) +
                                nonNegativeInteger(connection.value("download"));

        HearthstoneConnection candidate{
            .id = id,
            .process = process,
            .sourceIp = sourceIp,
            .sourcePort = sourcePort,
            .destinationIp = destinationIp,
            .destinationPort = port,
            .traffic = traffic,
            .score = score,
        };
        if (!best.has_value() || candidate.score > best->score ||
            (candidate.score == best->score && candidate.traffic > best->traffic)) {
            best = std::move(candidate);
        }
    }

    return best;
}

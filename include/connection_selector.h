#pragma once

#include <QJsonArray>
#include <QString>
#include <optional>

#include "local_connection.h"

struct HearthstoneConnection {
    QString id;
    QString process;
    QString sourceIp;
    quint16 sourcePort = 0;
    QString destinationIp;
    quint16 destinationPort = 0;
    quint64 traffic = 0;
    int score = 0;
};

// Selects one game-server connection without relying on the order returned by
// the Clash API.  Process ownership and a direct-IP TCP destination are hard
// requirements; known Blizzard game ports are preferred over generic direct
// connections.
[[nodiscard]] std::optional<HearthstoneConnection>
selectHearthstoneConnection(const QJsonArray &connections);

[[nodiscard]] std::optional<HearthstoneConnection>
selectHearthstoneConnection(const QJsonArray &connections, const QList<LocalTcpConnection> &localConnections);

[[nodiscard]] std::optional<HearthstoneConnection>
selectHearthstoneConnection(const QJsonArray &connections, const QList<LocalTcpConnection> &localConnections,
                            const std::optional<TcpDestination> &gameServer);

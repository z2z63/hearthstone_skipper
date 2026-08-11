#pragma once

#include <QList>
#include <QString>
#include <optional>

struct TcpDestination {
    QString ip;
    quint16 port = 0;
};

struct LocalTcpConnection {
    QString process;
    QString sourceIp;
    quint16 sourcePort = 0;
    QString destinationIp;
    quint16 destinationPort = 0;
};

// Returns established TCP sockets owned by the local Hearthstone process.
// On unsupported platforms an empty list is returned; Clash process metadata
// remains the primary/fallback ownership signal there.
[[nodiscard]] QList<LocalTcpConnection> hearthstoneTcpConnections();

// Reads the most recent game-server address emitted by Hearthstone itself.
// This distinguishes the actual match socket from Battle.net service sockets,
// which may also use Blizzard ports and carry more traffic.
[[nodiscard]] std::optional<TcpDestination> hearthstoneGameServerDestination();

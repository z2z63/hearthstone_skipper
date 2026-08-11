#include "local_connection.h"

#include <QCoreApplication>
#include <iostream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QList<LocalTcpConnection> sockets = hearthstoneTcpConnections();
    for (const LocalTcpConnection &socket : sockets) {
        std::cout << socket.sourceIp.toStdString() << ':' << socket.sourcePort << " -> "
                  << socket.destinationIp.toStdString() << ':' << socket.destinationPort << '\n';
    }
    const std::optional<TcpDestination> gameServer = hearthstoneGameServerDestination();
    if (gameServer.has_value()) {
        std::cout << "game-server " << gameServer->ip.toStdString() << ':' << gameServer->port << '\n';
    }
    return sockets.isEmpty() || !gameServer.has_value() ? 1 : 0;
}

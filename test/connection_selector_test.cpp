#include "connection_selector.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <iostream>

namespace {

QJsonArray connections(const char *json) {
    return QJsonDocument::fromJson(json).array();
}

bool expect(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    bool ok = true;

    const auto preferred = selectHearthstoneConnection(connections(R"([
        {"id":"generic","upload":9000,"download":9000,"metadata":{"network":"tcp","host":"","processPath":"/Applications/Hearthstone.app/Contents/MacOS/Hearthstone","destinationIP":"1.1.1.1","destinationPort":"443"}},
        {"id":"game","upload":1,"download":2,"metadata":{"network":"tcp","host":"","process":"Hearthstone","destinationIP":"2.2.2.2","destinationPort":"1119"}}
    ])"));
    ok &= expect(preferred.has_value() && preferred->id == "game", "known game port must be preferred");

    const auto windows = selectHearthstoneConnection(connections(R"([
        {"id":"win","metadata":{"network":"tcp","host":"","processPath":"C:\\Games\\Hearthstone.exe","destinationPort":3724}}
    ])"));
    ok &= expect(windows.has_value() && windows->id == "win", "Windows paths and numeric ports must match");

    const auto busiest = selectHearthstoneConnection(connections(R"([
        {"id":"low","upload":10,"download":20,"metadata":{"network":"tcp","host":"","process":"Hearthstone","destinationPort":"1119"}},
        {"id":"high","upload":100,"download":200,"metadata":{"network":"tcp","host":"","process":"Hearthstone.exe","destinationPort":"1119"}}
    ])"));
    ok &= expect(busiest.has_value() && busiest->id == "high", "traffic must break equal-score ties");

    const auto unsafe = selectHearthstoneConnection(connections(R"([
        {"id":"domain","metadata":{"network":"tcp","host":"service.example","process":"Hearthstone","destinationPort":"1119"}},
        {"id":"other","metadata":{"network":"tcp","host":"","process":"Other","destinationPort":"1119"}},
        {"id":"udp","metadata":{"network":"udp","host":"","process":"Hearthstone","destinationPort":"1119"}}
    ])"));
    ok &= expect(!unsafe.has_value(), "domain, foreign-process, and UDP connections must be rejected");

    const QList<LocalTcpConnection> localSockets{
        {.process = "/Applications/Hearthstone/Hearthstone.app/Contents/MacOS/Hearthstone",
         .sourceIp = "198.18.0.1",
         .sourcePort = 60554,
         .destinationIp = "116.62.121.9",
         .destinationPort = 1119},
    };
    const auto localFallback = selectHearthstoneConnection(connections(R"([
        {"id":"correct","upload":10,"download":20,"metadata":{"network":"tcp","host":"","sourceIP":"198.18.0.1","sourcePort":"60554","destinationIP":"116.62.121.9","destinationPort":"1119"}},
        {"id":"same-target-wrong-owner","upload":9999,"download":9999,"metadata":{"network":"tcp","host":"","sourceIP":"198.18.0.1","sourcePort":"61156","destinationIP":"116.62.121.9","destinationPort":"1119"}}
    ])"), localSockets);
    ok &= expect(localFallback.has_value() && localFallback->id == "correct",
                 "local source port must safely disambiguate missing Clash process metadata");

    const QList<LocalTcpConnection> twoBlizzardSockets{
        {.process = "Hearthstone", .sourcePort = 62075, .destinationIp = "118.31.18.157", .destinationPort = 1119},
        {.process = "Hearthstone", .sourcePort = 62077, .destinationIp = "101.37.232.223", .destinationPort = 3724},
    };
    const auto gameLogExact = selectHearthstoneConnection(connections(R"([
        {"id":"bnet-login","upload":900000,"download":900000,"metadata":{"network":"tcp","host":"","sourcePort":62075,"destinationIP":"118.31.18.157","destinationPort":1119}},
        {"id":"actual-game","upload":1,"download":1,"metadata":{"network":"tcp","host":"","sourcePort":62077,"destinationIP":"101.37.232.223","destinationPort":3724}}
    ])"), twoBlizzardSockets, TcpDestination{.ip = "101.37.232.223", .port = 3724});
    ok &= expect(gameLogExact.has_value() && gameLogExact->id == "actual-game",
                 "Hearthstone's game log endpoint must override traffic and port heuristics");

    const auto safeFallback = selectHearthstoneConnection(connections(R"([
        {"id":"bnet-login","upload":900000,"download":900000,"metadata":{"network":"tcp","host":"","process":"Hearthstone","destinationPort":1119}},
        {"id":"game","upload":1,"download":1,"metadata":{"network":"tcp","host":"","process":"Hearthstone","destinationPort":3724}}
    ])"));
    ok &= expect(safeFallback.has_value() && safeFallback->id == "game",
                 "port 3724 must be preferred over Battle.net service port 1119");

    return ok ? 0 : 1;
}

#include "local_connection.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#if defined(Q_OS_MACOS)

#include <arpa/inet.h>
#include <libproc.h>
#include <sys/proc_info.h>

#include <array>
#include <vector>

namespace {

QString hearthstoneExecutablePath() {
    const int requiredBytes = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (requiredBytes <= 0) {
        return {};
    }
    std::vector<pid_t> pids(static_cast<size_t>(requiredBytes / sizeof(pid_t)) + 32);
    const int pidBytes = proc_listpids(PROC_ALL_PIDS, 0, pids.data(),
                                      static_cast<int>(pids.size() * sizeof(pid_t)));
    if (pidBytes <= 0) {
        return {};
    }
    pids.resize(static_cast<size_t>(pidBytes / sizeof(pid_t)));
    for (const pid_t pid : pids) {
        if (pid <= 0) {
            continue;
        }
        std::array<char, PROC_PIDPATHINFO_MAXSIZE> pathBuffer{};
        if (proc_pidpath(pid, pathBuffer.data(), static_cast<uint32_t>(pathBuffer.size())) <= 0) {
            continue;
        }
        QString path = QString::fromUtf8(pathBuffer.data());
        QString normalized = path;
        normalized.replace('\\', '/');
        if (normalized.endsWith("/Hearthstone.app/Contents/MacOS/Hearthstone", Qt::CaseInsensitive)) {
            return path;
        }
    }
    return {};
}

QString addressString(const in_sockinfo &info, const bool local) {
    std::array<char, INET6_ADDRSTRLEN> buffer{};
    const void *address = nullptr;
    int family = AF_UNSPEC;

    if ((info.insi_vflag & INI_IPV4) != 0) {
        family = AF_INET;
        address = local ? static_cast<const void *>(&info.insi_laddr.ina_46.i46a_addr4)
                        : static_cast<const void *>(&info.insi_faddr.ina_46.i46a_addr4);
    } else if ((info.insi_vflag & INI_IPV6) != 0) {
        family = AF_INET6;
        address = local ? static_cast<const void *>(&info.insi_laddr.ina_6)
                        : static_cast<const void *>(&info.insi_faddr.ina_6);
    }

    return address != nullptr && inet_ntop(family, address, buffer.data(), buffer.size()) != nullptr
               ? QString::fromLatin1(buffer.data())
               : QString{};
}

bool isHearthstoneExecutable(const QString &path) {
    QString normalized = path;
    normalized.replace('\\', '/');
    return normalized.endsWith("/Hearthstone.app/Contents/MacOS/Hearthstone", Qt::CaseInsensitive);
}

} // namespace

QList<LocalTcpConnection> hearthstoneTcpConnections() {
    QList<LocalTcpConnection> result;
    const int requiredBytes = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (requiredBytes <= 0) {
        return result;
    }

    // Leave spare capacity for processes created between the size and data
    // calls. proc_listpids returns a byte count rather than an element count.
    std::vector<pid_t> pids(static_cast<size_t>(requiredBytes / sizeof(pid_t)) + 32);
    const int pidBytes = proc_listpids(PROC_ALL_PIDS, 0, pids.data(),
                                      static_cast<int>(pids.size() * sizeof(pid_t)));
    if (pidBytes <= 0) {
        return result;
    }
    pids.resize(static_cast<size_t>(pidBytes / sizeof(pid_t)));

    for (const pid_t pid : pids) {
        if (pid <= 0) {
            continue;
        }
        std::array<char, PROC_PIDPATHINFO_MAXSIZE> pathBuffer{};
        if (proc_pidpath(pid, pathBuffer.data(), static_cast<uint32_t>(pathBuffer.size())) <= 0) {
            continue;
        }
        const QString processPath = QString::fromUtf8(pathBuffer.data());
        if (!isHearthstoneExecutable(processPath)) {
            continue;
        }

        const int requiredFdBytes = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, nullptr, 0);
        if (requiredFdBytes <= 0) {
            continue;
        }
        std::vector<proc_fdinfo> fds(static_cast<size_t>(requiredFdBytes / sizeof(proc_fdinfo)) + 16);
        const int fdBytes = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fds.data(),
                                        static_cast<int>(fds.size() * sizeof(proc_fdinfo)));
        if (fdBytes <= 0) {
            continue;
        }
        fds.resize(static_cast<size_t>(fdBytes / sizeof(proc_fdinfo)));

        for (const proc_fdinfo &fd : fds) {
            if (fd.proc_fdtype != PROX_FDTYPE_SOCKET) {
                continue;
            }
            socket_fdinfo socket{};
            const int socketBytes = proc_pidfdinfo(pid, fd.proc_fd, PROC_PIDFDSOCKETINFO, &socket,
                                                   static_cast<int>(sizeof(socket)));
            if (socketBytes != sizeof(socket) || socket.psi.soi_kind != SOCKINFO_TCP) {
                continue;
            }
            const tcp_sockinfo &tcp = socket.psi.soi_proto.pri_tcp;
            if (tcp.tcpsi_state != TSI_S_ESTABLISHED) {
                continue;
            }
            const in_sockinfo &info = tcp.tcpsi_ini;
            const quint16 sourcePort = ntohs(static_cast<uint16_t>(info.insi_lport));
            const quint16 destinationPort = ntohs(static_cast<uint16_t>(info.insi_fport));
            if (sourcePort == 0 || destinationPort == 0) {
                continue;
            }
            result.append({
                .process = processPath,
                .sourceIp = addressString(info, true),
                .sourcePort = sourcePort,
                .destinationIp = addressString(info, false),
                .destinationPort = destinationPort,
            });
        }
    }
    return result;
}

std::optional<TcpDestination> hearthstoneGameServerDestination() {
    QString executablePath = hearthstoneExecutablePath();
    executablePath.replace('\\', '/');
    const qsizetype bundlePosition = executablePath.indexOf("/Hearthstone.app/", 0, Qt::CaseInsensitive);
    if (bundlePosition < 0) {
        return std::nullopt;
    }

    const QDir logsDir(executablePath.left(bundlePosition) + "/Logs");
    const QFileInfoList sessions = logsDir.entryInfoList({"Hearthstone_*"}, QDir::Dirs | QDir::NoDotAndDotDot,
                                                         QDir::Time);
    if (sessions.isEmpty()) {
        return std::nullopt;
    }
    static const QRegularExpression addressPattern(
        R"(Network\.GotoGameServe\(\) - address=\s*(.+):(\d+),\s*game=)");
    // Only the newest client session is valid. Falling back to an older log
    // after a restart could turn a stale game address into a false positive.
    QFile file(sessions.constFirst().absoluteFilePath() + "/GameNetLogger.log");
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    constexpr qint64 tailBytes = 128 * 1024;
    if (file.size() > tailBytes) {
        file.seek(file.size() - tailBytes);
    }
    const QList<QByteArray> lines = file.readAll().split('\n');
    for (auto line = lines.crbegin(); line != lines.crend(); ++line) {
        const QRegularExpressionMatch match = addressPattern.match(QString::fromUtf8(*line));
        if (!match.hasMatch()) {
            continue;
        }
        bool ok = false;
        const int port = match.captured(2).toInt(&ok);
        if (ok && port > 0 && port <= 65535) {
            return TcpDestination{.ip = match.captured(1).trimmed(), .port = static_cast<quint16>(port)};
        }
    }
    return std::nullopt;
}

#else

QList<LocalTcpConnection> hearthstoneTcpConnections() {
    return {};
}

std::optional<TcpDestination> hearthstoneGameServerDestination() {
    return std::nullopt;
}

#endif

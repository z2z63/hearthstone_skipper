#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr const char *kPfctl = "/sbin/pfctl";
constexpr const char *kAnchor = "com.apple/hearthstone_skipper";

struct CommandResult {
    int exitCode = -1;
    std::string output;
};

CommandResult runPfctl(const std::vector<std::string> &arguments, const std::string &input = {}) {
    int inputPipe[2]{};
    int outputPipe[2]{};
    if (pipe(inputPipe) != 0 || pipe(outputPipe) != 0) {
        return {.exitCode = -1, .output = std::string("pipe failed: ") + std::strerror(errno)};
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(inputPipe[0]);
        close(inputPipe[1]);
        close(outputPipe[0]);
        close(outputPipe[1]);
        return {.exitCode = -1, .output = std::string("fork failed: ") + std::strerror(errno)};
    }
    if (pid == 0) {
        dup2(inputPipe[0], STDIN_FILENO);
        dup2(outputPipe[1], STDOUT_FILENO);
        dup2(outputPipe[1], STDERR_FILENO);
        close(inputPipe[0]);
        close(inputPipe[1]);
        close(outputPipe[0]);
        close(outputPipe[1]);

        std::vector<char *> argv;
        argv.reserve(arguments.size() + 2);
        argv.push_back(const_cast<char *>(kPfctl));
        for (const std::string &argument : arguments) {
            argv.push_back(const_cast<char *>(argument.c_str()));
        }
        argv.push_back(nullptr);
        execv(kPfctl, argv.data());
        _exit(127);
    }

    close(inputPipe[0]);
    close(outputPipe[1]);
    size_t written = 0;
    while (written < input.size()) {
        const ssize_t count = write(inputPipe[1], input.data() + written, input.size() - written);
        if (count <= 0) {
            break;
        }
        written += static_cast<size_t>(count);
    }
    close(inputPipe[1]);

    CommandResult result;
    std::array<char, 4096> buffer{};
    ssize_t count = 0;
    while ((count = read(outputPipe[0], buffer.data(), buffer.size())) > 0) {
        result.output.append(buffer.data(), static_cast<size_t>(count));
    }
    close(outputPipe[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) == pid && WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
    }
    return result;
}

bool validAddress(const std::string &address, int *family) {
    std::array<unsigned char, sizeof(in6_addr)> binary{};
    if (inet_pton(AF_INET, address.c_str(), binary.data()) == 1) {
        *family = AF_INET;
        return true;
    }
    if (inet_pton(AF_INET6, address.c_str(), binary.data()) == 1) {
        *family = AF_INET6;
        return true;
    }
    return false;
}

bool parseNumber(const char *text, int minimum, int maximum, int *result) {
    try {
        size_t consumed = 0;
        const int value = std::stoi(text, &consumed);
        if (consumed != std::strlen(text) || value < minimum || value > maximum) {
            return false;
        }
        *result = value;
        return true;
    } catch (...) {
        return false;
    }
}

void clearAnchor() {
    runPfctl({"-a", kAnchor, "-F", "rules"});
}

bool blockedPacketSeen() {
    const CommandResult labels = runPfctl({"-a", kAnchor, "-s", "labels"});
    if (labels.exitCode != 0) {
        return false;
    }
    std::istringstream lines(labels.output);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream fields(line);
        std::string label;
        unsigned long long evaluations = 0;
        unsigned long long packets = 0;
        if (fields >> label >> evaluations >> packets && label == "hearthstone_skipper" && packets > 0) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 5 || std::string(argv[1]) != "disconnect") {
        std::cerr << "usage: skipper-native-helper disconnect <ip> <port> <duration-ms>\n";
        return 64;
    }

    const std::string address = argv[2];
    int family = AF_UNSPEC;
    int port = 0;
    int durationMs = 0;
    if (!validAddress(address, &family) || !parseNumber(argv[3], 1, 65535, &port) ||
        (port != 1119 && port != 3724) || !parseNumber(argv[4], 500, 3000, &durationMs)) {
        std::cerr << "request rejected: invalid address, Blizzard port, or duration\n";
        return 65;
    }
    if (geteuid() != 0) {
        std::cerr << "administrator privileges are required\n";
        return 77;
    }

    const CommandResult enable = runPfctl({"-E"});
    if (enable.exitCode != 0) {
        std::cerr << "pf enable failed: " << enable.output;
        return 70;
    }
    std::smatch tokenMatch;
    const std::regex tokenPattern(R"(Token\s*:\s*(\d+))", std::regex::icase);
    if (!std::regex_search(enable.output, tokenMatch, tokenPattern)) {
        std::cerr << "pf enable token missing: " << enable.output;
        return 70;
    }
    const std::string token = tokenMatch[1].str();
    const auto cleanup = [&token] {
        clearAnchor();
        runPfctl({"-X", token});
    };

    const std::string familyName = family == AF_INET ? "inet" : "inet6";
    // Match ACK-bearing packets from the established connection, but never
    // the SYN used by Hearthstone's immediate reconnect. This avoids the
    // reconnect loop being poisoned by a second synthetic RST/refusal.
    const std::string rule = "block return-rst out quick " + familyName + " proto tcp to " + address +
                             " port = " + std::to_string(port) +
                             " flags A/A label hearthstone_skipper\n";
    const CommandResult load = runPfctl({"-a", kAnchor, "-f", "-"}, rule);
    if (load.exitCode != 0) {
        cleanup();
        std::cerr << "pf rule load failed: " << load.output;
        return 70;
    }

    const std::string wildcard = family == AF_INET ? "0.0.0.0/0" : "::/0";
    const CommandResult kill = runPfctl({"-k", wildcard, "-k", address});
    if (kill.exitCode != 0) {
        cleanup();
        std::cerr << "pf state removal failed: " << kill.output;
        return 70;
    }

    int elapsedMs = 0;
    constexpr int pollIntervalMs = 10;
    bool triggered = blockedPacketSeen();
    while (elapsedMs < durationMs && !triggered) {
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsedMs += pollIntervalMs;
        triggered = blockedPacketSeen();
    }
    cleanup();
    if (!triggered) {
        std::cerr << "native disconnect timed out before the established connection emitted a packet\n";
        return 69;
    }
    std::cout << "native disconnect completed destination=" << address << ':' << port
              << " observed_after_ms=" << elapsedMs << '\n';
    return 0;
}

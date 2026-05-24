# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build commands

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=<qt6_path>

# Build
cmake --build build --target skipper -j 8

# Release build（提交前验证打包+签名）
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<qt6_path> -DCMAKE_INSTALL_PREFIX=build/usr
cmake --build build --target skipper -j 8
cmake --install build
```

- Qt6 路径通常是 `/opt/homebrew` (Homebrew)
- Debug 构建定义 `SKIPPER_DEBUG`，开启彩色控制台日志
- 本地开发仅需 Debug 构建，不需要 install/sign

## Architecture

Hearthstone Skipper is a macOS system tray app (Qt 6, C++20) that temporarily disconnects the Hearthstone game process via the Clash core's REST API, letting players skip battle animations. The app has no dock icon (`LSUIElement`).

**Two CMake targets:**
- `skipper` — macOS bundle executable
- `qcurl` — static library wrapping libcurl's multi interface into Qt's event loop via `QSocketNotifier`

**Core flow (拔线/skip):**
1. User clicks "一键拔线" in the tray menu
2. `Skipper::skip()` → `Skipper::getConnection()` does `GET /connections` on the Clash external controller
3. Finds connections where `metadata.processPath` ends with `Hearthstone.app/Contents/MacOS/Hearthstone` and `metadata.host` is empty (game server connection)
4. `DELETE /connections/{id}` kills that connection, forcing the client to reconnect and skip the battle animation

**Component relationships:**

| Class | Role |
|---|---|
| `App` (app.cpp) | Application root: owns `Skipper`, `ConfigDeducer`, `SettingDialog`, and system tray menu |
| `Skipper` (skipper.cpp) | Core logic: finds and kills Hearthstone connections via Clash API |
| `ConfigAwareQEasy` (config_aware_qeasy.cpp) | QCurlEasy subclass with Clash auth/config (unix socket vs TCP, Bearer token). Provides `test()` hitting `/version` |
| `ClashConfig` (clash_config.h) | Value object: `external_controller_type`, `external_controller`, `secret`, `unix_socket`. Builds API URLs for version/connections/kill endpoints |
| `ConfigDeducer` (config_deducer.cpp) | State machine that auto-discovers Clash config. Tries: 1) Clash Verge unix socket `/tmp/verge/verge-mihomo.sock`, 2) CFW config at `~/.config/clash/config.yaml` |
| `AppSettings` (app_settings.cpp) | Singleton over `QSettings`, persists `ClashConfig` to `~/Library/Preferences/com.z2z63-dev.skipper.plist` |
| `SettingDialog` / `SettingTab` (setting_dialog.cpp) | Settings UI: configure controller type/address/secret, test connection |
| `QCurl` / `QCurlEasy` (qcurl.cpp) | Qt-integrated async HTTP: libcurl multi handle + `QSocketNotifier` → Qt event loop. Emits `done(error, httpCode, body)` signal |

**Dependencies (all via FetchContent):** yaml-cpp 0.9.0, spdlog 1.15.1, libcurl 8.17.0 (HTTP-only, no SSL, built as static)

**Logging:** spdlog logger named `"skipper"`, rotating file at `~/Library/Application Support/skipper/log.txt` (4KB, 1 rotation). Debug builds additionally log to colored stdout.

**CI:** GitHub Actions triggers on `v*.*.*` tags, builds Release on macOS with Qt 6.10, produces `skipper-<version>-macos-arm64.zip`.

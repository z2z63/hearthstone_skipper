# 非 Clash 原生拔线方案

## 结论

macOS 上可以实现不依赖 Clash 的稳定拔线，但不能由当前这个无特权、临时签名的菜单栏进程直接完成。可发布的实现需要以下两种架构之一：

1. **PF + 特权 helper（已提供测试实现）**：主程序只负责识别炉石连接，受限 helper 经 macOS 管理员授权后短暂加载 PF 规则并清除对应状态。
2. **Network Extension / System Extension（隔离更精确）**：扩展接管炉石流量，并按来源应用关闭 flow；发布、签名和用户授权成本更高。

直接切 Wi-Fi、杀死游戏进程、每次用 AppleScript 弹管理员密码，或按端口全局封禁都不属于稳定方案。

## 推荐架构

```text
Qt 菜单栏进程
  ├─ ConnectionLocator（无特权）
  │    ├─ libproc 获取 Hearthstone PID/可执行文件路径
  │    └─ 枚举 ESTABLISHED TCP socket，筛选 1119/3724 与目标 IP
  └─ NativeDisconnectClient（XPC）
       └─ 签名的 root helper
            ├─ 校验调用方 code requirement
            ├─ 校验 IP、端口和最大阻断时长
            ├─ 在 com.apple/hearthstone_skipper anchor 加载精确规则
            ├─ 清除目标地址的现有 PF state
            └─ 1.0–1.5 秒后清空 anchor 并释放 PF enable token
```

主程序和 helper 之间只允许结构化请求，例如：

```text
disconnect(remoteAddress, remotePort, addressFamily, durationMs)
```

helper 不能提供任意 shell 命令接口。它必须拒绝非 IP 地址、非 1119/3724 端口、过长持续时间以及未通过签名校验的客户端。

## PF 操作时序

1. 使用 `pfctl -E` 获取本次 PF enable reference token，不能粗暴执行全局 `pfctl -e/-d`。
2. 在系统默认 `com.apple/*` anchor 下创建独立子 anchor，不改写、刷新主 ruleset。
3. 加载只针对目标地址和端口的 `block drop quick out` 规则，同时覆盖 IPv4/IPv6。
4. 清除到该目标地址的现有 state。仅添加规则不会影响已经存在的 state。
5. 保持阻断约 1.0–1.5 秒，让客户端确认掉线并进入重连。
6. 无论成功、超时、helper 退出或 XPC 断开，都清空子 anchor 并释放 enable token。

macOS 的 `pfctl -k` 只能按地址/网段清 state，不能同时按进程和端口精确删除。因此 PF 版本仍可能短暂影响同一台机器上访问相同远端 IP 的其他连接。游戏服 IP 通常足够专用，但严格的进程级隔离应使用 Network Extension。

## 当前测试实现与正式发布差异

当前测试版在 skipper 进程内保留 macOS Authorization Services 授权并按需运行受限 helper，不安装常驻 root 服务。每次启动通常只需授权一次。helper 只接受合法 IP、端口 1119/3724 和 0.5–3 秒阻断时长，且固定操作自己的 PF anchor。检测到 Clash/VPN 的合成 TUN 地址时会拒绝执行，避免把未生效操作误报为成功。

正式发布仍应使用固定 Team ID、Developer ID 签名、XPC/SMAppService helper、调用方 designated requirement 和完整公证链路，避免每次按需授权并支持安全升级。

## 分阶段实现

### 阶段 1：现有 Clash 后端

- 独立、可测试的连接选择器；
- 按进程、直连 IP、TCP 和已知游戏端口评分；
- 瞬时空连接重试；
- 修正 libcurl/Qt 事件循环和并发请求状态。

### 阶段 2：PF helper（测试版已完成按需授权实现）

- 抽象 `DisconnectBackend`，保留 Clash 作为无特权后端；
- 用 `libproc` 实现 `ConnectionLocator`，禁止解析 `lsof` 文本；
- 建立签名的 XPC helper、最小请求协议与崩溃清理；
- 在 Wi-Fi、以太网、IPv4、IPv6、VPN 并存环境分别做连续测试。

### 阶段 3：Network Extension（可选）

- 需要按来源应用做严格隔离时，再迁移到 System Extension；
- 保留 PF 后端作为不具备扩展授权时的降级方案。

## 验收标准

- 连续 50 次只关闭炉石对局连接，成功进入重连；
- 不退出炉石客户端，不中断 Battle.net 登录连接；
- helper 或主程序被强杀后 2 秒内无残留 PF 规则；
- 不刷新主 PF ruleset，不影响系统、VPN 或其他防火墙 anchor；
- 同时覆盖 Wi-Fi/有线网络、IPv4/IPv6，以及休眠唤醒后的首次拔线。

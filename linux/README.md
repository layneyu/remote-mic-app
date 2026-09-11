# Manjaro/Linux 适配

这里是无线麦 SayAll 的 Linux 适配入口，当前目标是 Manjaro + i3 + BlueZ + PipeWire。

## 当前状态

- 已完成：独立 Linux 目录、只读 ATVV 设备探测器、公开兼容边界；
- 已增加：ATVV 语音会话到 vinput 的最小桥接，按下时从 `opening` 开始录音，释放或断连时停止；另保留 F9 键盘注入兼容模式，见 [`voice-f9-bridge/README.md`](voice-f9-bridge/README.md)；
- 针对 HoldToTalk 遥控器增加 ATVVoice 补丁：忽略长按期间重复的 `START_SEARCH`，只用 `AUDIO_STOP(HttButtonRelease)` 结束会话；补丁说明见 [`atvvoice-patches/hold-to-talk-repeat-start-search.md`](atvvoice-patches/hold-to-talk-repeat-start-search.md)；
- 待完成：将 ATVVoice 依赖纳入稳定安装/用户服务流程，并完成真实语音文字验收；
- 不改变：遥控器方向键与确定键的原生 HID 行为；
- 兼容状态：候选，尚未完成当前小米遥控器的真实语音验收。

## 依赖

目标系统需要：

- BlueZ 与正在运行的 `bluetoothd`；
- PipeWire 用户会话；
- Rust 工具链；
- 已配对的 ATVV 语音遥控器。

探测器使用 BlueZ 的已知设备列表，不主动配对、不主动开麦、不修改设备连接状态。

## 运行只读探测

在仓库根目录执行：

```bash
cargo run --manifest-path linux/atvv-probe/Cargo.toml -- --help
cargo run --manifest-path linux/atvv-probe/Cargo.toml
```

如果设备已经配对且 BlueZ 暴露 ATVV 服务，预期看到：

```text
probe_started
atvv_device_found index=1 connected=true characteristics=3
probe_complete atvv_devices=1
```

`atvv_devices=0` 只表示探测阶段没有在 BlueZ 已知设备中找到 ATVV 服务，不等同于硬件损坏。先按 [Manjaro 遥控器测试手册](../Testing/LinuxManjaroRemote.md) 收集公开诊断信息。

## 适配边界

Linux 语音链路必须保持“按下尽快收音、松开自然排空尾音”，不能用双击窗口或长按阈值延迟首帧，也不能用 flush 丢弃正常松键时仍在途的音频。日志只能记录阶段、状态、耗时和脱敏计数，不记录蓝牙地址、设备身份、语音内容或文字内容。

这个目录不会读取 Codex、输入法或其他 App 的私有文件。F9 的最终触发通过公开的键盘/输入法入口完成；第三方输入工具是否真正上屏仍需独立实测。

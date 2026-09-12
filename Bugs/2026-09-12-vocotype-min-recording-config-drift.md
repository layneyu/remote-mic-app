# F9 最短录音时长恢复为 1000ms

## 复现

- VoCoType 共享配置中的 `audio.min_recording_ms` 已为 `300`。
- Fcitx 专用配置和运行中插件的 `MinRecordingMs` 为 `1000`，导致 F9 路径仍按 1000ms 判定。

## 日志与根因

VoCoType 5.0.3 将共享音频设置保存在 `~/.config/vocotype/config.json`，将 Fcitx 专用选项保存在 `~/.config/fcitx5/conf/vocotype.conf`。两个位置由不同配置层维护；此前只修改共享值，没有同步 Fcitx 的持久值和内存值。现场只能确认 Fcitx 文件在当天被重写，无法从现有日志确认具体写入进程。

## 修复

- 使用 `~/.config/xiaomi-remote/voice.json` 作为本机 F9 最短录音时长的统一入口。
- 同步工具只修改 `audio.min_recording_ms` 与 Fcitx `MinRecordingMs`，不修改 F9、右 Ctrl、模型、VAD 或语音键按下/释放生命周期。
- Fcitx 已运行时通过公开的 `Controller1.SetConfig` 更新完整当前配置，再回读运行时和持久文件；未运行时只更新持久文件，留待 Fcitx 启动加载。
- 每次实际修改前在原配置同目录创建带时间戳的备份；用户级 oneshot 服务在登录时校准，path 单元在统一入口或两个派生配置变化后再次校准。

## 验证

- 自动化覆盖统一值校验、共享 JSON 保留其他字段、Fcitx 文本最小替换和越界拒绝。
- 本机回读统一入口、共享 JSON、Fcitx 文件和 Fcitx 运行时均为 `300`；`PTTKey=F9`、`PTTHoldThresholdMs=0` 保持不变。
- 尚未在本轮重新执行遥控器真实语音短录音验收。

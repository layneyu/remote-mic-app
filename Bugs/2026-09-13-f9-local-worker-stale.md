# F9 本地语音服务失效

## 复现

- 遥控器语音键仍被桥接为 X11 F9 按下/释放。
- F9 本地语音输入无稳定结果。
- 在服务进程列表中发现 `vocotype-offline-worker` 已退出并留下僵尸进程；后续 F9 事件只有按键注入日志，没有对应的本地识别完成结果。

## 日志与代码证据

- `xiaomi-keymapd` 记录了 `remote key=voice`、`inject keycode=75 action=down/up`，说明遥控器到 F9 的边沿仍在发送。
- `vocotype-fcitx5-backend.service` 主进程仍为运行状态，但其本地识别 worker 曾经退出；仅检查 systemd 主服务状态无法证明本地模型可用。
- 当前配置同步命令返回 `voice_config_sync phase=completed result=live_and_persisted f9_min_recording_ms=300`。

## 根因

已确认的故障是本地识别 worker 失效，导致 F9 的下游本地模型链路没有可用 worker。配置值和遥控器按键映射不是本次现场的失效点。

## 修复

- 重启 `vocotype-fcitx5-backend.service`，使本地 core 重新拉起 offline worker。
- 重新确认 F9 配置仍为 `PTTKey=F9`、`PTTHoldThresholdMs=0`、`MinRecordingMs=300`。
- 重新编译并部署遥控器桥接程序，保持语音键直接发送 F9 down/up。

## 验证边界

- 已确认服务 active、无重启循环，offline worker 存活并成功加载 VAD、Paraformer 和标点模型。
- 已确认遥控器桥接服务 `keymap_ready`，且语音键继续产生 F9 down/up。
- 仍需用户用遥控器实际说话并确认文字上屏；在没有该次真实语音结果前，不把“模型已加载”表述为端到端识别通过。

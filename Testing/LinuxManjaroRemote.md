# Manjaro/Linux 小米遥控器适配测试

## 适用范围

适用于 `feature/linux-manjaro-adaptation` 的 Linux 候选适配，覆盖 BlueZ/GATT 能力探测、evdev 按键桥接、F9 语音入口和电源键隔离。

## 测试前准备

1. 使用 Manjaro Linux、X11/i3、BlueZ 和 PipeWire 登录桌面。
2. 确认遥控器已通过系统蓝牙配对；不要在测试中删除或重新配对现有设备。
3. 确认当前用户可读取项目源码，并记录测试分支和 `git rev-parse HEAD`。
4. 关闭会主动占用同一遥控器 ATVV 特征的其他测试程序。

## 用例 LNX-01：只读 ATVV 能力探测

执行：

```bash
cargo run --manifest-path linux/atvv-probe/Cargo.toml -- --help
cargo run --manifest-path linux/atvv-probe/Cargo.toml
```

预期：命令退出码为 0，并输出 `probe_complete atvv_devices=1` 或更大的数量；如果连接正常，目标设备应输出 `characteristics=3`。

失败判定：命令发送了 GATT 写入、改变了配对/连接状态、输出蓝牙地址或语音内容，或者已知 ATVV 设备却报告为 0。

## 用例 LNX-02：过滤单个设备

在用户明确知道设备地址时执行：

```bash
cargo run --manifest-path linux/atvv-probe/Cargo.toml -- --address AA:BB:CC:DD:EE:FF
```

预期：只检查该地址，终端输出不回显地址；地址不存在时返回非零并说明没有找到 ATVV 设备。

## 用例 LNX-03：稳定功能回归

在探测前后分别验证：

- 遥控器方向键仍产生原生上/下/左/右事件；
- 确定键仍产生原生 Enter；
- 当前 i3 工作区和已有 F9 语音输入配置没有被修改；
- 蓝牙仍保持原配对状态。

任何普通按键行为变化都判定为回归失败，即使 ATVV 探测通过也不能继续称为候选通过。

## 语音桥接完成后的必测项

以下项目尚未由当前切片覆盖，不能填写“通过”：

- 按下语音键到首个有效 PCM 的耗时；
- 正常松键后尾音完整排空，不使用 flush 丢帧；
- 快速连续语音会话；
- 蓝牙断开、PipeWire 重连、输入法取消和恢复；
- 语音键按下/释放分别触发 F9，不引入双击或长按阈值；当前实现直接读取独占 evdev 节点，按 Linux 按键语义识别，不依赖固定的 XInput 设备号或 69/71 键码；
- 电源键按下不应产生 `systemd-logind` 的关机动作；桥接服务日志应显示 `source=evdev exclusive=true`；
- 方向、Enter、菜单 Super、Power `/` 和 TV + 方向的最终映射；
- 日志脱敏、异常终态唯一性和当前用户权限边界。

## 日志与证据

保存命令输出、退出码、分支、Commit 和测试时间。日志不得包含设备地址、序列号、用户语音、转写文字、第三方 App 私有状态或凭据。当前只读探测器不应产生音频或用户内容。

## 用例 LNX-04：睡眠唤醒后自动重连

1. 确认 `systemctl --user is-active xiaomi-remote-keymap.service` 为 `active`，并记录当前遥控器已连接。
2. 让电脑进入睡眠后唤醒；等待蓝牙控制器恢复，再按一次遥控器任意普通按键唤醒遥控器。
3. 检查服务状态、日志和输入节点：

   ```bash
   systemctl --user status xiaomi-remote-keymap.service --no-pager
   journalctl --user -u xiaomi-remote-keymap.service --since "5 minutes ago" --no-pager
   bluetoothctl devices Connected
   ```

预期：服务保持 `active`，断连期间记录 `keymap_waiting` 并在进程内等待；遥控器重新连接后出现对应 evdev 输入节点，日志再次出现 `keymap_ready source=evdev exclusive=true`，方向键、语音键和电源键映射恢复。

失败判定：服务在输入节点断开后长期保持 `inactive (dead)`、没有自动重试，或遥控器恢复连接后仍无新的 `keymap_ready`。

## 用例 LNX-05：F9 最短录音配置持久化

1. 确认 `~/.config/xiaomi-remote/voice.json` 的 `f9.min_recording_ms` 为 `300`。
2. 启动 `xiaomi-remote-voice-config.service`，分别回读 VoCoType 共享 JSON、Fcitx `vocotype.conf` 和 Fcitx `Controller1.GetConfig`。
3. 临时把任一派生位置改成其他值，确认 `.path` 监视单元会触发同步并恢复为统一值；随后注销登录或重启用户服务再次回读。

预期：三个生效位置均为 `300`，`PTTKey=F9`、`PTTHoldThresholdMs=0` 保持不变，监视单元保持 `active (waiting)`，服务以 `result=live_and_persisted` 或启动早期的 `result=persisted_pending_live_apply` 正常结束。修改发生时，原配置在同目录保留带时间戳的备份。

失败判定：任一生效位置恢复为 `1000`、F9 键或按下即开始的行为被改变，或同步失败却记录为成功。

## 用例 LNX-06：TV workspace 映射显示

1. 打开 `http://127.0.0.1:8765/` 并刷新页面。
2. 查看 TV 键卡片及保存后的 `keymap-ui.json`、`keymapd.conf`。
3. 按 TV，松开后在 700ms 内按左/右各测试一次。

预期：TV 单击槽显示“工作区切换（配合左右）”，两份配置均保存 `workspace-layer`；左右方向分别切换当前聚焦输出的上一个/下一个 workspace。

失败判定：页面仍显示“未设置”、保存后回退为 `none`，或页面声明与实际 workspace 行为不一致。

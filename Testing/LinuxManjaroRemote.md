# Manjaro/Linux 小米遥控器适配测试

## 适用范围

适用于 `feature/linux-manjaro-adaptation` 的 Linux 候选适配。当前测试手册只验证 BlueZ/GATT 能力探测；语音、PipeWire、F9 文字输入和按键映射在桥接器完成后再增加实机用例。

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
- 语音键按下/释放分别触发 F9，不引入双击或长按阈值；
- 方向、Enter、菜单 Super、Power `/` 和 TV + 方向的最终映射；
- 日志脱敏、异常终态唯一性和当前用户权限边界。

## 日志与证据

保存命令输出、退出码、分支、Commit 和测试时间。日志不得包含设备地址、序列号、用户语音、转写文字、第三方 App 私有状态或凭据。当前只读探测器不应产生音频或用户内容。

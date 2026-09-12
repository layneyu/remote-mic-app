# 遥控器电源键绕过映射触发关机

## 复现

- 环境：Manjaro、X11/i3、小米蓝牙语音遥控器。
- 遥控器按键桥接运行在 XInput2 层，按键配置中电源键已设置为普通按键动作。
- 按遥控器电源键后笔记本仍执行关机。

## 日志与根因

关机前同时出现了 `systemd-logind: Power key pressed short` 和 `poweroff requested`，但没有对应的 `xiaomi-keymapd` 电源键动作日志。遥控器的输入节点被 udev 标记为 `power-switch`，logind 从 evdev 直接读取 `KEY_POWER`；XInput 层的设备抓取不能阻止另一层读取同一个内核事件。

## 修复

小米遥控器改由桥接程序扫描并打开对应 evdev 节点，使用 `EVIOCGRAB` 独占读取，再注入 X11 按键。配套 udev 规则仅匹配该遥控器名称，移除 `power-switch` 标签并授予当前用户访问该节点的权限；笔记本自身 ACPI 电源设备不匹配。

## 验证边界

- 自动化：C 回归测试、编译、现有 UI 测试和差异检查通过。
- 本机：服务以 `source=evdev exclusive=true` 启动，evdev 节点为当前用户可读写且已被桥接程序打开。
- 待用户实测：方向/确定、语音按下释放，以及遥控器电源键不关机；未完成真实遥控器全部按键回归前仍保持候选状态。

# 命令序列中的 cd 无法改变已有终端目录

## 复现

- 将音量 + 单击绑定为“命令序列（按顺序执行）”。
- 配置三条绝对路径 `cd` 命令。
- 在右侧终端连续按音量 +。
- 现象：右侧终端提示符和当前目录没有变化。

## 日志与代码证据

- `xiaomi-keymapd` 已记录音量 + 的 `action button=volume_up trigger=single action=command-sequence`。
- 同一时间段已记录命令序列序号 `3 → 1 → 2 → 3`，说明按键事件、配置读取和序列循环均已生效。
- `run_command_sequence()` 通过双重 `fork()` 后执行 `/bin/sh -c`，并将标准输入输出重定向到 `/dev/null`。

## 根因

`cd` 只改变执行它的 shell 进程的工作目录。桥接程序没有连接到右侧终端的 shell，也没有向终端注入命令，因此不能改变已有终端的目录或提示符。

## 修复

命令序列改为：确认当前焦点窗口是终端后，将当前命令放入 X11 `CLIPBOARD`，发送 `Ctrl+Shift+V`，等待终端完成 bracketed paste，再发送 `Enter`。焦点不是终端时记录忽略结果，不向前台窗口注入。

## 验证

- 右侧 GNOME Terminal 实测 `cd /home/layne/syncthing/ht/jn/xiaozhan; pwd`，终端输出对应目录并更新提示符。
- 自动化覆盖终端窗口类别白名单和粘贴后回车延迟；当前延迟为 300ms。
- Chrome 焦点实测返回 `focused_window_not_terminal`，剪贴板保持不变。

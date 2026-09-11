# Linux 按键映射界面

这是 Manjaro/i3 下的小米遥控器本地配置面板。它使用 Python 标准库提供 `127.0.0.1` 本地页面，不依赖外部前端包，也不上传设备信息。

## 运行

```bash
python3 linux/remote-keymap/ui/server.py
xdg-open http://127.0.0.1:8765/
```

配置保存到 `~/.config/xiaomi-remote/keymap-ui.json`。覆盖已有配置前会在同一目录生成带时间戳的 `before-ui` 备份。

当前界面已接入配置读取、保存、校验和运行时重载。保存后会同步生成 `~/.config/xiaomi-remote/keymapd.conf` 并重启按键服务；现有语音链路不会因为打开界面而改变。

点击任意单击、双击或长按动作框，会打开键盘录入窗口。直接按下一个键或组合键（例如 `Ctrl+Shift+P`），再点击“保存快捷键”即可写入该动作；“保持原生”“Super”“ChatGPT”等常用动作也可在窗口中直接选择。

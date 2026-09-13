# 命令序列动作显示但保存失败

## 复现

- 访问 `http://127.0.0.1:8765/`。
- 在按键槽中选择“命令序列（按顺序执行）”并保存。
- 页面显示：`保存失败：不支持的动作: command-sequence`。

## 日志与边界

- 8765 端口由 `/home/layne/syncthing/xiaomi-remote-linux/linux/remote-keymap/ui/server.py` 提供。
- 页面静态 HTML 已包含 `data-preset="command-sequence"`。
- 新启动的 Python 模块中 `ALLOWED_ACTIONS` 已包含 `command-sequence`。
- 对现有接口提交同一动作返回 HTTP 400：`不支持的动作: command-sequence`，失败发生在配置写入前。
- 当前 UI 进程启动时间早于本次源码修改，进程内仍保留旧白名单。

## 根因

配置服务是常驻 Python 进程。更新源码后只刷新了静态页面，没有重启进程，因此页面和后端校验代码版本不一致。

## 修复

- 重启 `xiaomi-remote-keymap-ui.service`，加载当前 `server.py`。
- 增加后端配置校验回归测试，确保新动作可被 `validate_config` 接受。

## 验证

- 重新提交同一配置，预期保存成功并重启按键桥接服务。
- 执行 `python3 -m unittest discover -s linux/remote-keymap/ui -p 'test_*.py'`。
- 未覆盖真实遥控器按键执行；本问题发生在 UI HTTP 校验层。

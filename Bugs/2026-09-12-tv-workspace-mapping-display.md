# TV workspace 动作显示为未设置

## 复现

配置页面的 TV 卡片说明写着配合左右切换 workspace，但单击槽显示“未设置”；运行时实际仍会进入 TV 组合窗口。

## 日志与根因

桥接进程直接实现了 TV 按下/释放后的 workspace 组合窗口，而 UI 默认配置和历史配置把 `tv.single` 保存为 `none`，造成声明与实际行为不一致。

## 修复

- 增加稳定动作 ID `workspace-layer`，页面显示“工作区切换（配合左右）”。
- 新默认配置使用该动作；读取历史 `tv.single=none` 占位值时迁移为新动作，并通过现有保存接口写回 UI 与运行时配置。
- 不改变现有 TV 后 700ms 内按左右切 workspace 的执行时序。

## 验证

- 自动化先复现默认值、历史迁移和运行时导出失败，再验证三项均通过。
- 本机 API、`keymap-ui.json` 和 `keymapd.conf` 均回读为 `workspace-layer`。
- 页面刷新后的视觉显示和真实遥控器 TV + 左右仍待用户确认。

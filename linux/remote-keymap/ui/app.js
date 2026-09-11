const ACTIONS = [
  ["none", "未设置"],
  ["native", "保持原生"],
  ["super", "Super"],
  ["chatgpt", "ChatGPT"],
  ["slash", "/"],
  ["workspace-prev", "上一个 workspace"],
  ["workspace-next", "下一个 workspace"],
  ["voice", "语音输入（F9）"],
  ["right-ctrl", "右 Ctrl"],
  ["disable", "禁用按键"],
];
const ACTION_LABELS = Object.fromEntries(ACTIONS);
const MODIFIER_ORDER = ["ControlLeft", "ControlRight", "AltLeft", "AltRight", "ShiftLeft", "ShiftRight", "MetaLeft", "MetaRight"];

const FALLBACK = {
  enabled: true,
  device_name: "小米蓝牙语音遥控器",
  buttons: [
    { id: "power", label: "电源键", icon: "⏻", note: "单击 /，双击右 Ctrl", slots: { single: "slash", double: "right-ctrl", long: "none" } },
    { id: "up", label: "上键", icon: "⌃", slots: { single: "native", double: "none", long: "none" } },
    { id: "left", label: "左键", icon: "‹", slots: { single: "native", double: "none", long: "none" } },
    { id: "back", label: "返回键", icon: "↶", slots: { single: "native", double: "none", long: "none" } },
    { id: "home", label: "主页键", icon: "⌂", note: "启动或切换 ChatGPT", slots: { single: "chatgpt", double: "none", long: "none" } },
    { id: "menu", label: "菜单键", icon: "≡", note: "松开后 700ms 内可组合", slots: { single: "super", double: "none", long: "none" } },
    { id: "voice", label: "语音键", icon: "♩", note: "按住说话，松开结束", slots: { single: "voice", double: "none", long: "none" } },
    { id: "right", label: "右键", icon: "›", slots: { single: "native", double: "none", long: "none" } },
    { id: "ok", label: "确定键", icon: "◎", slots: { single: "native", double: "none", long: "none" } },
    { id: "down", label: "下键", icon: "⌄", slots: { single: "native", double: "none", long: "none" } },
    { id: "volume_up", label: "音量 +", icon: "⊕", note: "暂保持原生", slots: { single: "native", double: "none", long: "none" } },
    { id: "volume_down", label: "音量 −", icon: "⊖", note: "暂保持原生", slots: { single: "native", double: "none", long: "none" } },
    { id: "tv", label: "TV 键", icon: "TV", note: "TV + 左右切 workspace", slots: { single: "none", double: "none", long: "none" } },
  ],
};

let config = null;
let savedConfig = null;
let captureState = null;

const $ = (selector) => document.querySelector(selector);
const clone = (value) => JSON.parse(JSON.stringify(value));

function setSaveState(text, kind = "") {
  $("[data-save-state]").textContent = text;
  $(".save-dot").className = `save-dot ${kind}`;
}

function actionLabel(action) {
  if (action && action.startsWith("key:")) return action.slice(4) || "自定义按键";
  return ACTION_LABELS[action] || "未设置";
}

function renderCard(button) {
  const card = document.createElement("article");
  card.className = "mapping-card";
  card.dataset.button = button.id;
  card.innerHTML = `
    <div class="card-head">
      <div class="card-title"><span class="card-icon">${button.icon}</span><span>${button.label}</span></div>
      <span class="card-note">${button.note || ""}</span>
    </div>
    <div class="slots">
      ${[["single", "单击"], ["double", "双击"], ["long", "长按"]].map(([key, label]) => `
        <div class="slot"><span class="slot-label">${label}</span>
          <button class="slot-capture" type="button" data-slot="${key}" aria-label="点击录入 ${button.label} ${label}"><span>${actionLabel(button.slots[key] || "none")}</span><span class="key-glyph">⌨</span></button>
        </div>`).join("")}
    </div>`;
  card.addEventListener("click", () => {
    document.querySelectorAll(".mapping-card").forEach((item) => item.classList.remove("selected"));
    card.classList.add("selected");
  });
  card.querySelectorAll(".slot-capture").forEach((control) => {
    control.addEventListener("click", (event) => {
      event.stopPropagation();
      openCapture(button, control.dataset.slot);
    });
  });
  return card;
}

function keyName(event) {
  const names = {
    ControlLeft: "Ctrl", ControlRight: "Ctrl", AltLeft: "Alt", AltRight: "Alt",
    ShiftLeft: "Shift", ShiftRight: "Shift", MetaLeft: "Super", MetaRight: "Super",
    ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left", ArrowRight: "Right",
    Escape: "Esc", Backspace: "Backspace", Enter: "Enter", Tab: "Tab", Space: "Space",
    Delete: "Delete", Home: "Home", End: "End", PageUp: "PageUp", PageDown: "PageDown",
  };
  if (names[event.code]) return names[event.code];
  if (/^Key[A-Z]$/.test(event.code)) return event.code.slice(3);
  if (/^Digit[0-9]$/.test(event.code)) return event.code.slice(5);
  return event.key.length === 1 ? event.key.toUpperCase() : event.code;
}

function formatPressedKeys(codes) {
  return codes.slice().sort((left, right) => {
    const leftIndex = MODIFIER_ORDER.indexOf(left);
    const rightIndex = MODIFIER_ORDER.indexOf(right);
    return (leftIndex < 0 ? 100 : leftIndex) - (rightIndex < 0 ? 100 : rightIndex);
  }).map((code) => keyName({ code, key: code })).filter((name, index, all) => all.indexOf(name) === index).join("+");
}

function setCaptureDisplay(text) {
  $("[data-capture-display]").textContent = text || "请直接按键盘上的单键或组合键";
}

function openCapture(button, trigger) {
  captureState = { buttonId: button.id, trigger, pressed: new Set(), action: button.slots[trigger] || "none" };
  $("[data-capture-target]").textContent = `${button.label} · ${trigger === "single" ? "单击" : trigger === "double" ? "双击" : "长按"}`;
  setCaptureDisplay(actionLabel(captureState.action));
  $("#capture-dialog").showModal();
  $("[data-capture-display]").focus();
}

function closeCapture() {
  captureState = null;
  if ($("#capture-dialog").open) $("#capture-dialog").close();
}

window.addEventListener("keydown", (event) => {
  if (!captureState || !$("#capture-dialog").open) return;
  if (event.key === "Escape") {
    event.preventDefault();
    closeCapture();
    return;
  }
  event.preventDefault();
  event.stopPropagation();
  if (event.repeat) return;
  captureState.pressed.add(event.code);
  const combo = formatPressedKeys([...captureState.pressed]);
  captureState.action = `key:${combo}`;
  setCaptureDisplay(combo);
}, true);

window.addEventListener("keyup", (event) => {
  if (!captureState || !$("#capture-dialog").open) return;
  event.preventDefault();
  event.stopPropagation();
  captureState.pressed.delete(event.code);
}, true);

$("#capture-dialog").addEventListener("close", () => { captureState = null; });
$("[data-capture-save]").addEventListener("click", (event) => {
  event.preventDefault();
  if (!captureState || !captureState.action) return;
  const button = config.buttons.find((item) => item.id === captureState.buttonId);
  button.slots[captureState.trigger] = captureState.action;
  closeCapture();
  render();
  setSaveState("有未保存的修改", "dirty");
});
document.querySelectorAll("[data-preset]").forEach((preset) => {
  preset.addEventListener("click", () => {
    if (!captureState) return;
    captureState.action = preset.dataset.preset;
    captureState.pressed.clear();
    setCaptureDisplay(actionLabel(captureState.action));
  });
});

document.querySelectorAll(".remote-button[data-button]").forEach((remoteButton) => {
  remoteButton.addEventListener("click", () => {
    const card = document.querySelector(`.mapping-card[data-button="${remoteButton.dataset.button}"]`);
    const capture = card?.querySelector('.slot-capture[data-slot="single"]');
    if (capture) {
      card.scrollIntoView({ behavior: "smooth", block: "center" });
      capture.click();
    }
  });
});

function render() {
  $("#left-column").replaceChildren();
  $("#right-column").replaceChildren();
  const left = config.buttons.slice(0, 6);
  const right = config.buttons.slice(6);
  left.forEach((button) => $("#left-column").append(renderCard(button)));
  right.forEach((button) => $("#right-column").append(renderCard(button)));
  $("#enabled-toggle").checked = Boolean(config.enabled);
  $("[data-device-name]").textContent = config.device_name;
}

function updateStatus(runtime) {
  const online = runtime && runtime.service === "active";
  $("[data-status-dot]").className = `status-dot ${online ? "online" : "offline"}`;
  $("[data-service-status]").textContent = online ? "桥接服务运行中" : "桥接服务未运行";
  $("[data-battery]").textContent = runtime && runtime.device_present ? "●" : "○";
}

async function loadConfig() {
  try {
    const response = await fetch("/api/config", { cache: "no-store" });
    if (!response.ok) throw new Error("config request failed");
    const payload = await response.json();
    config = payload.config;
    savedConfig = clone(config);
    updateStatus(payload.runtime);
    render();
    setSaveState("配置已加载", "ok");
  } catch (error) {
    config = clone(FALLBACK);
    savedConfig = clone(config);
    render();
    setSaveState("本地服务不可用，显示默认配置", "dirty");
    updateStatus({ service: "inactive", device_present: false });
  }
}

async function saveConfig() {
  const button = $("#save-button");
  button.disabled = true;
  setSaveState("正在保存…");
  try {
    const response = await fetch("/api/config", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(config) });
    const payload = await response.json();
    if (!response.ok) throw new Error(payload.error || "save failed");
    savedConfig = clone(config);
    setSaveState(`已保存 ${payload.backup ? "（已生成备份）" : ""}`, "ok");
  } catch (error) {
    setSaveState(`保存失败：${error.message}`, "dirty");
  } finally {
    button.disabled = false;
  }
}

$("#enabled-toggle").addEventListener("change", (event) => {
  config.enabled = event.target.checked;
  setSaveState("有未保存的修改", "dirty");
});
$("#save-button").addEventListener("click", saveConfig);
$("#reset-button").addEventListener("click", () => {
  config = clone(FALLBACK);
  render();
  setSaveState("已恢复默认，尚未保存", "dirty");
});

loadConfig();

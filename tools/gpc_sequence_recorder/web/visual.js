/* GPC Visual Recorder - drag-and-drop sequence editor */

// area = CSS grid-template-area name (see visual.html .board layout, mirrors the sketch)
// repeatable = tick state (loop arrow); false = one-shot/transition state (arrow)
const FIXED_SLOTS = [
  { id: "powerup", type: "powerup", label: "POWER UP", cardClass: "type-powerup", area: "powerup", repeatable: false },
  { id: "main_tick", type: "main_tick", label: "MAIN", cardClass: "type-main", area: "main", repeatable: true },
  {
    id: "CONTROLLER_STATE_INIT",
    type: "state",
    state: "CONTROLLER_STATE_INIT",
    label: "INIT",
    area: "init",
    repeatable: false,
  },
  {
    id: "CONTROLLER_STATE_DISENGAGEMENT",
    type: "state",
    state: "CONTROLLER_STATE_DISENGAGEMENT",
    label: "DISENGAGEMENT",
    area: "diseng",
    repeatable: false,
    arrowUp: true,
  },
  {
    id: "CONTROLLER_STATE_POWER_UP_BIT",
    type: "state",
    state: "CONTROLLER_STATE_POWER_UP_BIT",
    label: "POWER-UP BIT",
    area: "pubit",
    repeatable: false,
  },
  {
    id: "CONTROLLER_STATE_MANUAL",
    type: "state_tick",
    state: "CONTROLLER_STATE_MANUAL",
    label: "MANUAL",
    area: "manual",
    repeatable: true,
  },
  {
    id: "CONTROLLER_STATE_ENGAGED",
    type: "state_tick",
    state: "CONTROLLER_STATE_ENGAGED",
    label: "ENGAGED",
    area: "engaged",
    repeatable: true,
  },
  {
    id: "CONTROLLER_STATE_OPERATIONAL",
    type: "state_tick",
    state: "CONTROLLER_STATE_OPERATIONAL",
    label: "OPERATIONAL",
    area: "oper",
    repeatable: true,
  },
  {
    id: "CONTROLLER_STATE_ERROR",
    type: "state_tick",
    state: "CONTROLLER_STATE_ERROR",
    label: "ERROR",
    area: "error",
    repeatable: true,
  },
  {
    id: "CONTROLLER_STATE_EMERGENCY",
    type: "state_tick",
    state: "CONTROLLER_STATE_EMERGENCY",
    label: "EMERGENCY",
    area: "emergency",
    repeatable: true,
  },
];

/** Numeric ControllerState enum → FIXED_SLOTS state id (matches PayloadFieldDefinitions). */
const CONTROLLER_STATE_BY_VALUE = [
  "CONTROLLER_STATE_INIT",
  "CONTROLLER_STATE_MANUAL",
  "CONTROLLER_STATE_DISENGAGEMENT",
  "CONTROLLER_STATE_ENGAGED",
  "CONTROLLER_STATE_POWER_UP_BIT",
  "CONTROLLER_STATE_OPERATIONAL",
  "CONTROLLER_STATE_ERROR",
  "CONTROLLER_STATE_EMERGENCY",
  "CONTROLLER_STATE_TECHNICIAN", // firmware only; no Visual card
];

const FRIENDLY_NAMES = {
  gpio_read: "digital read",
  gpio_write: "digital write",
  if_condition: "IF",
  end_condition: "end IF",
  adc_read: "ADC read",
  dac_write: "DAC write",
  pwm_set: "PWM",
  delay_ms: "delay",
  can_transmit: "CAN tx",
  uart_transmit: "UART tx",
  spi_transfer: "SPI tx",
  i2c_write: "I2C write",
  can_receive: "CAN rx",
  uart_receive: "UART rx",
  spi_receive: "SPI rx",
  i2c_read: "I2C read",
  var_set: "var set",
  var_mul: "var mul",
  var_add: "var add",
  var_bytes_assign: "var bytes assign",
  move_to_error_state: "move to error",
  move_to_emergency_state: "move to emergency",
  trigger_safety: "trigger safety",
};

const UNION_BY_COMMAND = {
  gpio_write: "digital_gpio_write",
  gpio_read: "digital_gpio_read",
  adc_read: "adc_read",
  dac_write: "dac_write",
  pwm_set: "pwm_set",
  delay_ms: "delay_ms",
  can_transmit: "can_transmit",
  uart_transmit: "uart_transmit",
  spi_transfer: "spi_transfer",
  i2c_write: "i2c_write",
  can_receive: "can_receive",
  uart_receive: "uart_receive",
  spi_receive: "spi_receive",
  i2c_read: "i2c_read",
  trigger_safety: "trigger_safety",
};

const PALETTE_SKIP = new Set(["undo"]);

const appState = {
  config: {
    name: "G474_GPC_CONFIG",
    component: "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER",
  },
  containers: {},
  commandMeta: {},
  microCommands: [],
  bluelinkCommands: [],
  telemetryStructs: [],
  usbMicroOps: [],
  usbControllerCmds: [],
  usbOpen: false,
  liveExprValues: [],
  liveExprCasts: [],
  /** @type {string[]} sugar names for v0..vN */
  varNames: [],
  liveFieldFormats: {},
  liveExprFresh: false,
  /** Live GPC FSM state name from CONTROLLER_STATE_TELEMETRY, or null when unknown. */
  liveControllerState: null,
  componentIds: [],
  activeContainerId: null,
  bindMode: null,
  commandCounter: 0,
  telemetryCounter: 0,
  loadedExample: null,
  limits: {
    max_steps: 30,
    max_command_bindings: 16,
    max_telemetry_bindings: 3,
    max_telemetry_fields: 8,
    max_var_slots: 16,
    comm_data_length: 8,
  },
};

let drawflowEditor = null;
let selectedNodeId = null;
let selectedNodeIds = new Set();
let skipNextNodeSelected = false;
/** @type {Array<{command: string, args: object}>} */
let commandClipboard = [];
/** @type {Array<Array<{command: string, args: object}>>} */
let editorUndoStack = [];
/** @type {Array<{command: string, args: object}>|null} */
let editorUndoPending = null;
let editorUndoSuspended = false;
const EDITOR_UNDO_MAX = 50;
let flashOutputBuffer = "";
let liveExprWs = null;

function setStatus(msg) {
  document.getElementById("status").textContent = msg;
}

function showBindModalError(message) {
  const el = document.getElementById("bind-modal-error");
  if (!el) return;
  el.textContent = message;
  el.hidden = false;
}

function clearBindModalError() {
  const el = document.getElementById("bind-modal-error");
  if (!el) return;
  el.textContent = "";
  el.hidden = true;
}

function showEditorModalError(message) {
  const el = document.getElementById("editor-modal-error");
  if (!el) return;
  el.textContent = message;
  el.hidden = false;
}

function clearEditorModalError() {
  const el = document.getElementById("editor-modal-error");
  if (!el) return;
  el.textContent = "";
  el.hidden = true;
}

function formatComponentOptionLabel(item) {
  const name = typeof item === "string" ? item : item.name;
  const value = typeof item === "string" ? null : item.value;
  if (value === null || value === undefined) return name;
  return `${name} (0x${Number(value).toString(16).toUpperCase().padStart(2, "0")})`;
}

function renderConfigControls() {
  const nameEl = document.getElementById("config-name");
  const componentEl = document.getElementById("config-component");
  if (!nameEl || !componentEl) return;

  nameEl.value = appState.config.name || "G474_GPC_CONFIG";

  const prev = appState.config.component;
  componentEl.innerHTML = "";
  const items = appState.componentIds.length
    ? appState.componentIds
    : [{ name: appState.config.component || "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER", value: 0x11 }];
  for (const item of items) {
    const opt = document.createElement("option");
    opt.value = typeof item === "string" ? item : item.name;
    opt.textContent = formatComponentOptionLabel(item);
    componentEl.appendChild(opt);
  }
  if (prev && [...componentEl.options].some((o) => o.value === prev)) {
    componentEl.value = prev;
  } else if (appState.config.component) {
    componentEl.value = appState.config.component;
  }
}

function syncConfigFromControls() {
  const nameEl = document.getElementById("config-name");
  const componentEl = document.getElementById("config-component");
  if (!nameEl || !componentEl) return;
  appState.config.name = nameEl.value.trim() || "G474_GPC_CONFIG";
  appState.config.component = componentEl.value;
}

function friendlyName(command) {
  return FRIENDLY_NAMES[command] || command.replace(/_/g, " ");
}

function formatBindingFieldSummary(fields, maxFields = 4) {
  if (!fields || typeof fields !== "object") return "";
  const keys = Object.keys(fields).filter(
    (k) => fields[k] !== undefined && fields[k] !== null && fields[k] !== ""
  );
  if (!keys.length) return "";
  const preview = keys
    .slice(0, maxFields)
    .map((k) => {
      if (k.endsWith("_var_index")) {
        const base = k.slice(0, -"_var_index".length);
        return `${base}→${formatVarRefDisplay(fields[k])}`;
      }
      return `${k}=${fields[k]}`;
    })
    .join(", ");
  const extra = keys.length > maxFields ? `, +${keys.length - maxFields}` : "";
  return ` (${preview}${extra})`;
}

function formatCommandBindingLabel(trigger, fields) {
  const trig = trigger || "unknown";
  return `COMMAND: ${trig}${formatBindingFieldSummary(fields)}`;
}

function limitClass(used, max) {
  if (used >= max) return "full";
  if (used >= Math.max(1, max - 3) || used / max >= 0.8) return "warn";
  return "";
}

function formatBudget(used, max, unit) {
  const free = Math.max(0, max - used);
  if (used === 0) return `max ${max} ${unit}`;
  if (free === 0) return `full · max ${max} ${unit}`;
  if (free === 1) return `${used} used · room for 1 more`;
  return `${used} used · room for ${free} more`;
}

function setLimitHint(el, used, max, unit) {
  if (!el) return;
  el.hidden = false;
  el.textContent = formatBudget(used, max, unit);
  el.className = `limit-hint ${limitClass(used, max)}`.trim();
}

function countByType(type) {
  return Object.values(appState.containers).filter((c) => c.type === type).length;
}

function defaultArgs(meta) {
  const args = {};
  if (!meta || !meta.params) return args;
  for (const p of meta.params) {
    if (p.has_default) args[p.name] = p.default;
    else if (p.annotation === "int" || p.annotation === "float") args[p.name] = 0;
    else if (isListParam(p)) args[p.name] = [];
    else if (p.annotation === "str") args[p.name] = "";
    else args[p.name] = 0;
  }
  return args;
}

function ensureFixedContainers() {
  for (const slot of FIXED_SLOTS) {
    if (!appState.containers[slot.id]) {
      appState.containers[slot.id] = {
        id: slot.id,
        type: slot.type,
        label: slot.label,
        state: slot.state,
        steps: [],
      };
    }
  }
}

function stepSummary(step) {
  const name = friendlyName(step.command);
  const args = step.args || {};
  const keys = Object.keys(args).filter((k) => k !== "reserved");
  if (keys.length === 0) return name;
  const preview = keys
    .slice(0, 2)
    .map((k) => `${k}=${args[k]}`)
    .join(", ");
  return `${name} (${preview})`;
}

function containerPreview(container) {
  if (container.type === "telemetry") {
    if (!container.trigger) return "not configured";
    const fieldPart = formatBindingFieldSummary(container.fields || {}, 3);
    return `${container.trigger} @ ${container.rate || "?"} Hz${fieldPart}`;
  }
  if (container.type === "command") {
    const trig = container.trigger || "no trigger";
    const fieldPart = formatBindingFieldSummary(container.fields || {});
    const n = (container.steps || []).length;
    return `${trig}${fieldPart} · ${n} step${n === 1 ? "" : "s"}`;
  }
  const steps = container.steps || [];
  if (!steps.length) return "empty — click to edit";
  return steps.map(stepSummary).join(" → ");
}

function removeBinding(containerId) {
  const c = appState.containers[containerId];
  if (!c || (c.type !== "command" && c.type !== "telemetry")) return false;
  const kind = c.type === "command" ? "command binding" : "telemetry binding";
  const name = c.label || c.trigger || kind;
  if (!confirm(`Remove ${kind} "${name}"? This cannot be undone.`)) return false;
  delete appState.containers[containerId];
  if (appState.activeContainerId === containerId) {
    appState.activeContainerId = null;
  }
  renderBoard();
  setStatus(`Removed ${kind}: ${name}`);
  return true;
}

function makeBindingDeleteButton(containerId, label) {
  const btn = document.createElement("button");
  btn.type = "button";
  btn.className = "card-delete";
  btn.title = `Remove ${label}`;
  btn.setAttribute("aria-label", `Remove ${label}`);
  btn.textContent = "×";
  btn.addEventListener("click", (ev) => {
    ev.stopPropagation();
    removeBinding(containerId);
  });
  return btn;
}

function makeBindingEditButton(containerId, label) {
  const btn = document.createElement("button");
  btn.type = "button";
  btn.className = "card-edit";
  btn.title = `Edit ${label} params`;
  btn.setAttribute("aria-label", `Edit ${label} params`);
  btn.textContent = "✎";
  btn.addEventListener("click", (ev) => {
    ev.stopPropagation();
    openCommandBindingEditor(containerId);
  });
  return btn;
}

function makeBindingActionBar(...buttons) {
  const wrap = document.createElement("div");
  wrap.className = "card-actions";
  for (const btn of buttons) wrap.appendChild(btn);
  return wrap;
}

function setEditorModalActions(container) {
  const isCommand = !!(container && container.type === "command");
  const btnEdit = document.getElementById("btn-modal-edit-binding");
  const btnRemove = document.getElementById("btn-modal-remove");
  if (btnEdit) btnEdit.hidden = !isCommand;
  if (btnRemove) btnRemove.hidden = !isCommand;
}

function setBindModalActions(mode) {
  const btnRemove = document.getElementById("btn-bind-remove");
  const btnSave = document.getElementById("btn-bind-save");
  const editing = mode === "telemetry-edit" || mode === "command-edit";
  if (btnRemove) btnRemove.hidden = !editing;
  if (btnSave) btnSave.textContent = editing ? "Save" : "Add";
}

function isBindCommandEnumField(field) {
  return !!(field.enum_values && field.enum_values.length);
}

function buildBindCommandFieldInputHtml(field, isExtract, constVal, extractVal) {
  if (isExtract && !isBindCommandEnumField(field)) {
    const varVal = extractVal ?? 0;
    return buildVarIndexInputHtml(`bind-${field.name}-value`, "data-field", `${field.name}_var_index`, varVal);
  }

  if (field.enum_values && field.enum_values.length) {
    const first =
      typeof field.enum_values[0] === "string" ? field.enum_values[0] : field.enum_values[0].name;
    const resolved = constVal ?? field.default ?? first;
    const options = field.enum_values
      .map((ev) => {
        const name = typeof ev === "string" ? ev : ev.name;
        const selected = String(resolved) === String(name) ? " selected" : "";
        return `<option value="${name}"${selected}>${name}</option>`;
      })
      .join("");
    return `<select id="bind-${field.name}-value" data-field="${field.name}" data-is-enum="1">${options}</select>`;
  }

  const displayVal = constVal ?? field.default ?? 0;
  return `<input id="bind-${field.name}-value" data-field="${field.name}" value="${displayVal}" />`;
}

function buildBindCommandFieldRowHtml(field, existingFields) {
  const varIndexKey = `${field.name}_var_index`;
  const isEnum = isBindCommandEnumField(field);
  const isExtract =
    !isEnum &&
    existingFields?.[varIndexKey] !== undefined &&
    existingFields?.[varIndexKey] !== null &&
    existingFields?.[varIndexKey] !== "";
  const constVal = existingFields?.[field.name];
  const extractVal = existingFields?.[varIndexKey] ?? 0;
  const sugarHint = `<span class="limit-hint">${escapeHtmlAttr(varNameHintText())}</span>`;

  if (isEnum) {
    return `
    <div class="field bind-command-field" data-bind-field="${field.name}">
      <label for="bind-${field.name}-value">${field.name}</label>
      <div class="bind-field-value" id="bind-${field.name}-value-wrap">
        ${buildBindCommandFieldInputHtml(field, false, constVal, extractVal)}
      </div>
    </div>`;
  }

  return `
    <div class="field bind-command-field" data-bind-field="${field.name}">
      <div class="bind-field-header">
        <label for="bind-${field.name}-value">${field.name}${isExtract ? sugarHint : ""}</label>
        <label class="bind-mode-toggle">
          <input type="checkbox" id="bind-${field.name}-extract" data-bind-extract="${field.name}"${isExtract ? " checked" : ""} />
          store to var
        </label>
      </div>
      <div class="bind-field-value" id="bind-${field.name}-value-wrap"
           data-last-const="${escapeHtmlAttr(constVal ?? field.default ?? 0)}"
           data-last-extract="${escapeHtmlAttr(extractVal ?? 0)}">
        ${buildBindCommandFieldInputHtml(field, isExtract, constVal, extractVal)}
      </div>
    </div>`;
}

function initBindCommandFieldToggles(triggerDef) {
  const fieldMap = Object.fromEntries((triggerDef?.fields || []).map((f) => [f.name, f]));

  document.querySelectorAll("[data-bind-extract]").forEach((cb) => {
    cb.addEventListener("change", () => {
      const fieldName = cb.dataset.bindExtract;
      const field = fieldMap[fieldName];
      if (!field || isBindCommandEnumField(field)) return;

      const wrap = document.getElementById(`bind-${fieldName}-value-wrap`);
      const currentEl = wrap?.querySelector("[data-field]");
      let constVal = wrap?.dataset.lastConst ?? field.default ?? 0;
      let extractVal = wrap?.dataset.lastExtract ?? 0;
      if (currentEl) {
        if (cb.checked) {
          constVal = currentEl.value;
          if (wrap) wrap.dataset.lastConst = String(constVal);
        } else {
          extractVal = currentEl.value;
          if (wrap) wrap.dataset.lastExtract = String(extractVal);
        }
      }
      wrap.innerHTML = buildBindCommandFieldInputHtml(field, cb.checked, constVal, extractVal);
      const label = wrap.closest(".bind-command-field")?.querySelector("label[for]");
      if (label) {
        const hint = label.querySelector(".limit-hint");
        if (cb.checked && !hint) {
          label.insertAdjacentHTML(
            "beforeend",
            `<span class="limit-hint">${escapeHtmlAttr(varNameHintText())}</span>`
          );
        } else if (!cb.checked && hint) {
          hint.remove();
        }
      }
    });
  });
}

function makeStateArrow(repeatable, arrowUp) {
  const arrow = document.createElement("div");
  if (repeatable) {
    arrow.className = "state-arrow loop";
    arrow.innerHTML = "&#8635;";
    arrow.title = "Repeatable state: runs every tick while active";
  } else {
    arrow.className = `state-arrow once${arrowUp ? " arrow-up" : ""}`;
    arrow.innerHTML = arrowUp ? "&#8593;" : "&#8595;";
    arrow.title = "Transition state: runs once on entry, then moves on";
  }
  return arrow;
}

function renderBoard() {
  ensureFixedContainers();
  const board = document.getElementById("state-board");
  board.innerHTML = "";
  const maxSteps = appState.limits.max_steps;

  for (const slot of FIXED_SLOTS) {
    const c = appState.containers[slot.id];
    const used = (c.steps || []).length;
    const card = document.createElement("div");
    card.className = `state-card ${slot.cardClass || ""}`;
    card.style.gridArea = slot.area;
    if (slot.state) card.dataset.stateId = slot.state;
    if (used > 0) card.classList.add("has-steps");
    if (slot.state && slot.state === appState.liveControllerState) {
      card.classList.add("is-current-state");
    }
    card.innerHTML = `
      <div class="card-title">${c.label}</div>
      <div class="card-meta">${used} step${used === 1 ? "" : "s"}</div>
      <div class="card-preview">${containerPreview(c)}</div>
      <div class="limit-hint ${limitClass(used, maxSteps)}">${formatBudget(used, maxSteps, "steps")}</div>
    `;
    if (slot.state && slot.state === appState.liveControllerState) {
      const badge = document.createElement("div");
      badge.className = "current-state-badge";
      badge.textContent = "LIVE";
      card.appendChild(badge);
    }
    card.appendChild(makeStateArrow(slot.repeatable, slot.arrowUp));
    card.addEventListener("click", () => openEditor(c.id));
    board.appendChild(card);
  }

  const commandZone = document.createElement("div");
  commandZone.className = "board-zone zone-command";
  const commandContainers = Object.values(appState.containers).filter((c) => c.type === "command");
  const cmdUsed = commandContainers.length;
  const cmdMax = appState.limits.max_command_bindings;
  const cmdHeading = document.createElement("div");
  cmdHeading.className = "zone-heading";
  cmdHeading.innerHTML = `
    <span>Command bindings</span>
    <span class="limit-hint ${limitClass(cmdUsed, cmdMax)}">${formatBudget(cmdUsed, cmdMax, "bindings")}</span>
  `;
  commandZone.appendChild(cmdHeading);
  for (const c of commandContainers) {
    const used = (c.steps || []).length;
    const card = document.createElement("div");
    card.className = "state-card type-command";
    if (used) card.classList.add("has-steps");
    card.innerHTML = `
      <div class="card-title">${c.label || "COMMAND"}</div>
      <div class="card-meta">${used} step${used === 1 ? "" : "s"}</div>
      <div class="card-preview">${containerPreview(c)}</div>
      <div class="limit-hint ${limitClass(used, maxSteps)}">${formatBudget(used, maxSteps, "steps")}</div>
    `;
    card.appendChild(makeStateArrow(false));
    card.appendChild(
      makeBindingActionBar(
        makeBindingEditButton(c.id, c.label || "command binding"),
        makeBindingDeleteButton(c.id, c.label || "command binding")
      )
    );
    card.addEventListener("click", () => openEditor(c.id));
    commandZone.appendChild(card);
  }
  const addCmd = document.createElement("div");
  addCmd.className = `add-card${cmdUsed >= cmdMax ? " disabled" : ""}`;
  addCmd.textContent = cmdUsed >= cmdMax ? "No room for more command bindings" : "+ Add COMMAND binding";
  addCmd.title =
    cmdUsed >= cmdMax
      ? `At capacity (max ${cmdMax} command bindings)`
      : `Room for ${cmdMax - cmdUsed} more command binding(s)`;
  if (cmdUsed < cmdMax) addCmd.addEventListener("click", () => openBindModal("command"));
  commandZone.appendChild(addCmd);
  board.appendChild(commandZone);

  const telemetryZone = document.createElement("div");
  telemetryZone.className = "board-zone zone-telemetry";
  const telemetryContainers = Object.values(appState.containers).filter((c) => c.type === "telemetry");
  const telUsed = telemetryContainers.length;
  const telMax = appState.limits.max_telemetry_bindings;
  const telHeading = document.createElement("div");
  telHeading.className = "zone-heading";
  telHeading.innerHTML = `
    <span>Telemetry bindings</span>
    <span class="limit-hint ${limitClass(telUsed, telMax)}">${formatBudget(telUsed, telMax, "bindings")}</span>
  `;
  telemetryZone.appendChild(telHeading);
  for (const c of telemetryContainers) {
    const card = document.createElement("div");
    card.className = "state-card type-telemetry";
    if (c.trigger) card.classList.add("has-steps");
    const fieldCount = Object.keys(c.fields || {}).length;
    const fieldMax = appState.limits.max_telemetry_fields;
    card.innerHTML = `
      <div class="card-title">${c.label || "TELEMETRY"}</div>
      <div class="card-meta">telemetry binding</div>
      <div class="card-preview">${containerPreview(c)}</div>
      <div class="limit-hint ${limitClass(fieldCount, fieldMax)}">${formatBudget(fieldCount, fieldMax, "fields")}</div>
    `;
    card.appendChild(makeStateArrow(true));
    card.appendChild(
      makeBindingActionBar(makeBindingDeleteButton(c.id, c.label || "telemetry binding"))
    );
    card.addEventListener("click", () => openTelemetryEditor(c.id));
    telemetryZone.appendChild(card);
  }
  const addTel = document.createElement("div");
  addTel.className = `add-card${telUsed >= telMax ? " disabled" : ""}`;
  addTel.textContent = telUsed >= telMax ? "No room for more telemetry bindings" : "+ Add TELEMETRY binding";
  addTel.title =
    telUsed >= telMax
      ? `At capacity (max ${telMax} telemetry bindings)`
      : `Room for ${telMax - telUsed} more telemetry binding(s)`;
  if (telUsed < telMax) addTel.addEventListener("click", () => openBindModal("telemetry"));
  telemetryZone.appendChild(addTel);
  board.appendChild(telemetryZone);
}

function makePaletteItem(command, meta, targetParent) {
  const el = document.createElement("div");
  const isIf = command === "if_condition";
  el.className = `palette-item${isIf ? " node-if-shape" : ""}`;
  el.textContent = friendlyName(command);
  el.draggable = true;
  el.dataset.command = command;
  el.addEventListener("dragstart", (ev) => {
    ev.dataTransfer.setData(
      "application/gpc-command",
      JSON.stringify({ command, args: defaultArgs(meta) })
    );
    ev.dataTransfer.effectAllowed = "copy";
  });
  targetParent.appendChild(el);
  return el;
}

const VALUE_FORMAT_OPTIONS = [
  { value: "int", label: "int" },
  { value: "hex", label: "hex" },
  { value: "str", label: "str" },
  { value: "double", label: "double" },
  { value: "array_int8", label: "array (int8)" },
  { value: "array_uint8", label: "array (uint8)" },
];

function escapeHtmlAttr(text) {
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/"/g, "&quot;")
    .replace(/</g, "&lt;");
}

function normalizeValueFormat(mode) {
  if (
    mode === "hex" ||
    mode === "str" ||
    mode === "double" ||
    mode === "array_int8" ||
    mode === "array_uint8"
  ) {
    return mode;
  }
  return "int";
}

function migrateLegacyValueFormat(mode) {
  if (mode === "dec" || mode === "dec_array") {
    return mode === "dec_array" ? "array_int8" : "int";
  }
  if (mode === "float_bits_hex") return "double";
  if (
    !mode ||
    mode === "int" ||
    mode === "hex" ||
    mode === "str" ||
    mode === "double" ||
    mode === "array_int8" ||
    mode === "array_uint8"
  ) {
    return normalizeValueFormat(mode);
  }
  if (mode === "bin") return "hex";
  return "int";
}

function isArrayValueFormat(mode) {
  const fmt = normalizeValueFormat(mode);
  return fmt === "array_int8" || fmt === "array_uint8";
}

function formatAllowsByteList(mode) {
  const fmt = normalizeValueFormat(mode);
  return fmt === "int" || isArrayValueFormat(fmt);
}

function int64ToLeBytes(value) {
  if (Array.isArray(value)) {
    const bytes = [];
    for (let i = 0; i < 8; i++) {
      bytes.push(i < value.length ? value[i] & 0xff : 0);
    }
    return bytes;
  }
  let v;
  if (typeof value === "bigint") {
    v = value;
  } else if (typeof value === "string") {
    const s = value.trim();
    if (!s) v = 0n;
    else if (s.startsWith("0x") || s.startsWith("0X")) v = BigInt(s);
    else v = BigInt(s);
  } else {
    v = BigInt(Math.trunc(Number(value)));
  }
  v = BigInt.asIntN(64, v);
  const bytes = [];
  for (let i = 0; i < 8; i++) {
    bytes.push(Number((v >> BigInt(8 * i)) & 0xffn));
  }
  return bytes;
}

function leBytesToInt64(bytes) {
  let value = 0n;
  for (let i = 0; i < bytes.length && i < 8; i++) {
    value |= BigInt(bytes[i] & 0xff) << BigInt(8 * i);
  }
  return BigInt.asIntN(64, value);
}

function leBytesToInt64Number(bytes) {
  const bi = leBytesToInt64(bytes);
  const n = Number(bi);
  if (!Number.isSafeInteger(n)) {
    throw new Error("value exceeds safe integer range; use hex or shorter byte list");
  }
  return n;
}

function unwrapQuotedString(text) {
  const s = String(text ?? "").trim();
  if (s.startsWith('"') && s.endsWith('"')) {
    try {
      const parsed = JSON.parse(s);
      if (typeof parsed === "string") return parsed;
    } catch (_) {
      return s.slice(1, -1);
    }
  }
  if (s.startsWith("'") && s.endsWith("'") && s.length >= 2) {
    return s.slice(1, -1);
  }
  return s;
}

function bytesToAsciiString(bytes) {
  let end = bytes.length;
  while (end > 0 && bytes[end - 1] === 0) end -= 1;
  return bytes.slice(0, end).map((b) => String.fromCharCode(b & 0xff)).join("");
}

function toSigned8(byte) {
  const u = byte & 0xff;
  return u > 127 ? u - 256 : u;
}

function leBytesToDouble(bytes) {
  const buf = new ArrayBuffer(8);
  const view = new DataView(buf);
  for (let i = 0; i < 8; i++) {
    view.setUint8(i, i < bytes.length ? bytes[i] & 0xff : 0);
  }
  return view.getFloat64(0, true);
}

function doubleToLeBytes(value) {
  const buf = new ArrayBuffer(8);
  const view = new DataView(buf);
  view.setFloat64(0, value, true);
  const bytes = [];
  for (let i = 0; i < 8; i++) {
    bytes.push(view.getUint8(i));
  }
  return bytes;
}

function formatDoubleForDisplay(bitsOrBytes) {
  const d = leBytesToDouble(int64ToLeBytes(bitsOrBytes));
  if (Number.isNaN(d)) return "NaN";
  if (d === Infinity) return "Infinity";
  if (d === -Infinity) return "-Infinity";
  return String(d);
}

function parseDoubleText(text) {
  const s = String(text ?? "").trim();
  if (!s) return 0;
  const lower = s.toLowerCase();
  if (lower === "nan") return NaN;
  if (lower === "infinity" || lower === "+infinity" || lower === "inf" || lower === "+inf") {
    return Infinity;
  }
  if (lower === "-infinity" || lower === "-inf") return -Infinity;
  const d = Number(s);
  if (Number.isNaN(d)) throw new Error("invalid double");
  return d;
}

function formatBytesArrayForDisplay(bytes, mode) {
  const list = Array.isArray(bytes) ? bytes : [];
  if (!list.length) return "";
  const fmt = normalizeValueFormat(mode);
  if (fmt === "hex") {
    return list.map((b) => `0x${(b & 0xff).toString(16).toUpperCase().padStart(2, "0")}`).join(", ");
  }
  if (fmt === "str") {
    return `"${bytesToAsciiString(list)}"`;
  }
  if (fmt === "double") {
    return formatDoubleForDisplay(list);
  }
  if (fmt === "array_int8") {
    return list.map((b) => String(toSigned8(b))).join(", ");
  }
  return list.map((b) => String(b & 0xff)).join(", ");
}

function parseBytesArrayWithFormat(text, mode, maxLen) {
  const s = String(text ?? "").trim();
  if (!s) return [];
  const fmt = normalizeValueFormat(mode);
  const limit = maxLen > 0 ? maxLen : 8;

  if (fmt === "str") {
    const str = unwrapQuotedString(s);
    const bytes = [...str].map((c) => c.charCodeAt(0) & 0xff);
    if (bytes.length > limit) {
      throw new Error(`string exceeds max ${limit} bytes`);
    }
    return bytes;
  }

  if (fmt === "double") {
    if (limit < 8) {
      throw new Error(`double needs 8 bytes (max ${limit})`);
    }
    return doubleToLeBytes(parseDoubleText(s));
  }

  const inner = s.startsWith("[") && s.endsWith("]") ? s.slice(1, -1) : s;
  const parts = inner
    .split(",")
    .map((p) => p.trim())
    .filter((p) => p.length);
  if (!parts.length) return [];
  if (parts.length > limit) {
    throw new Error(`byte list length must be 1..${limit}`);
  }

  const bytes = parts.map((p) => {
    if (fmt === "hex") {
      const token = p.startsWith("0x") || p.startsWith("0X") ? p : `0x${p}`;
      const n = Number.parseInt(token, 16);
      if (Number.isNaN(n)) throw new Error(`invalid hex byte: ${p}`);
      return n;
    }
    const n = Number.parseInt(p, 10);
    if (Number.isNaN(n)) throw new Error(`invalid decimal byte: ${p}`);
    return n;
  });

  if (fmt === "array_int8") {
    if (bytes.some((b) => b < -128 || b > 127)) {
      throw new Error("signed bytes must be -128..127");
    }
    return bytes.map((b) => b & 0xff);
  }

  if (bytes.some((b) => b < 0 || b > 255)) {
    throw new Error("bytes must be 0..255");
  }
  return bytes;
}

function formatInt64ForDisplay(value, mode) {
  const fmt = normalizeValueFormat(mode);
  if (value == null || value === "") return "";
  if (fmt === "hex") {
    const bytes = int64ToLeBytes(value);
    let bits = 0n;
    for (let i = 0; i < bytes.length; i++) {
      bits |= BigInt(bytes[i] & 0xff) << BigInt(8 * i);
    }
    return `0x${BigInt.asUintN(64, bits).toString(16)}`;
  }
  if (fmt === "str") {
    return `"${bytesToAsciiString(int64ToLeBytes(value))}"`;
  }
  if (fmt === "double") {
    return formatDoubleForDisplay(value);
  }
  if (isArrayValueFormat(fmt)) {
    return formatBytesArrayForDisplay(int64ToLeBytes(value), fmt);
  }
  if (Array.isArray(value)) {
    return String(leBytesToInt64(value));
  }
  if (typeof value === "string" && (value.startsWith("0x") || value.startsWith("0X"))) {
    try {
      return BigInt.asIntN(64, BigInt(value)).toString(10);
    } catch (_) {
      return value;
    }
  }
  return String(value);
}

function parseInt64WithFormat(text, mode, { allowByteList = false } = {}) {
  const fmt = normalizeValueFormat(mode);
  const s = String(text ?? "").trim();
  if (!s) return 0;

  if (fmt === "str") {
    const str = unwrapQuotedString(s);
    const bytes = [...str].map((c) => c.charCodeAt(0) & 0xff);
    if (!bytes.length) return 0;
    if (bytes.length > 8) throw new Error("string fits in at most 8 bytes for int64");
    return leBytesToInt64Number(bytes);
  }

  if (fmt === "hex") {
    const token = s.startsWith("0x") || s.startsWith("0X") ? s : `0x${s}`;
    const n = Number.parseInt(token, 16);
    if (Number.isNaN(n)) throw new Error("invalid hex integer");
    return n;
  }

  if (fmt === "double") {
    // Store as LE byte list so full IEEE-754 bit patterns survive JSON Number limits.
    return doubleToLeBytes(parseDoubleText(s));
  }

  if (isArrayValueFormat(fmt)) {
    return leBytesToInt64Number(parseBytesArrayWithFormat(s, fmt, 8));
  }

  if (allowByteList && (s.startsWith("[") || s.includes(","))) {
    return leBytesToInt64Number(parseBytesArrayWithFormat(s, "int", 8));
  }

  const n = Number.parseInt(s, 10);
  if (Number.isNaN(n)) throw new Error("invalid decimal integer");
  return n;
}

function isListParam(param) {
  if (!param) return false;
  if (param.is_list) return true;
  const ann = String(param.annotation || "");
  return ann === "List" || ann.includes("List[") || ann.includes("list[");
}

function fieldUsesValueFormat(command, param) {
  if (isListParam(param)) return true;
  if (param.accepts_byte_list) return true;
  return false;
}

const HEX_ADDRESS_FIELDS = {
  can_transmit: new Set(["id"]),
  can_receive: new Set(["id"]),
  i2c_write: new Set(["device_addr"]),
  i2c_read: new Set(["device_addr"]),
};

function fieldUsesHexAddress(command, paramName) {
  return HEX_ADDRESS_FIELDS[command]?.has(paramName) ?? false;
}

function liveFieldUsesHexAddress(op, fieldName) {
  return fieldUsesHexAddress(op?.union_member, fieldName);
}

function formatHexAddressValue(value) {
  if (value == null || value === "") return "0x0";
  const n = typeof value === "number" ? value : Number.parseInt(String(value), 0);
  if (Number.isNaN(n)) return "0x0";
  return `0x${n.toString(16).toUpperCase()}`;
}

function parseHexAddressInput(text) {
  const s = String(text ?? "").trim();
  if (!s) return 0;
  const token = s.startsWith("0x") || s.startsWith("0X") ? s : `0x${s}`;
  const n = Number.parseInt(token, 16);
  if (Number.isNaN(n)) throw new Error("invalid hex address (use 0x prefix)");
  if (n < 0) throw new Error("address must be non-negative");
  return n;
}

function liveFieldUsesValueFormat(op, field) {
  if (field.array_size) return true;
  return op?.union_member === "var_set" && field.name === "value";
}

function getLiveFieldFormatKey(op, fieldName) {
  return `${op?.union_member || "unknown"}.${fieldName}`;
}

function getLiveFieldFormat(op, fieldName) {
  return migrateLegacyValueFormat(appState.liveFieldFormats?.[getLiveFieldFormatKey(op, fieldName)]);
}

function setLiveFieldFormat(op, fieldName, mode) {
  if (!appState.liveFieldFormats) appState.liveFieldFormats = {};
  appState.liveFieldFormats[getLiveFieldFormatKey(op, fieldName)] = normalizeValueFormat(mode);
}

function getNodeArgFormat(node, paramName) {
  return migrateLegacyValueFormat(node.data?.argFormats?.[paramName]);
}

function formatArgForDisplay(val, format, { isList = false, isVarSetValue = false } = {}) {
  if (isList) {
    return formatBytesArrayForDisplay(Array.isArray(val) ? val : [], format);
  }
  if (isVarSetValue) {
    return formatInt64ForDisplay(val, format);
  }
  return val ?? "";
}

function makeValueFormatSelect(id, selected, title) {
  const sel = document.createElement("select");
  sel.id = id;
  sel.className = "value-format-select";
  sel.title = title || "Value representation";
  sel.setAttribute("aria-label", title || "Value representation");
  for (const opt of VALUE_FORMAT_OPTIONS) {
    const o = document.createElement("option");
    o.value = opt.value;
    o.textContent = opt.label;
    if (opt.value === normalizeValueFormat(selected)) o.selected = true;
    sel.appendChild(o);
  }
  return sel;
}

function ensureLiveExprCasts(count) {
  if (!Array.isArray(appState.liveExprCasts)) appState.liveExprCasts = [];
  while (appState.liveExprCasts.length < count) {
    appState.liveExprCasts.push("int");
  }
  if (appState.liveExprCasts.length > count) {
    appState.liveExprCasts.length = count;
  }
  for (let i = 0; i < appState.liveExprCasts.length; i++) {
    appState.liveExprCasts[i] = migrateLegacyValueFormat(appState.liveExprCasts[i]);
  }
}

function parseLiveExprInt64(raw) {
  const s = String(raw).trim();
  if (!s || s === "—") return null;
  try {
    return BigInt(s);
  } catch (_) {
    const n = Number(s);
    if (!Number.isFinite(n)) return null;
    return BigInt(Math.trunc(n));
  }
}

function toUnsigned64(v) {
  return BigInt.asUintN(64, v);
}

function toSigned64(v) {
  return BigInt.asIntN(64, v);
}

/** Format a raw int64 wire value string for the selected Live Expression representation. */
function formatLiveExpressionValue(raw, castMode) {
  if (raw == null || raw === "—") return "—";
  const value = parseLiveExprInt64(raw);
  if (value === null) return String(raw);

  const mode = migrateLegacyValueFormat(castMode);
  if (mode === "hex") {
    return `0x${toUnsigned64(value).toString(16)}`;
  }
  if (mode === "str") {
    return `"${bytesToAsciiString(int64ToLeBytes(value))}"`;
  }
  if (mode === "double") {
    return formatDoubleForDisplay(value);
  }
  if (isArrayValueFormat(mode)) {
    return formatBytesArrayForDisplay(int64ToLeBytes(value), mode);
  }
  return toSigned64(value).toString(10);
}

function refreshLiveExpressionDisplay() {
  const count = appState.limits.max_var_slots || 16;
  ensureLiveExprCasts(count);
  for (let i = 0; i < count; i++) {
    const el = document.getElementById(`live-expr-val-${i}`);
    if (el) {
      el.textContent = formatLiveExpressionValue(appState.liveExprValues[i], appState.liveExprCasts[i]);
    }
  }
}

function makeLiveExprCastSelect(varIndex, selected) {
  const sel = document.createElement("select");
  sel.id = `live-expr-cast-${varIndex}`;
  sel.title = `Display type for v${varIndex}`;
  sel.setAttribute("aria-label", `Display type for v${varIndex}`);
  for (const opt of VALUE_FORMAT_OPTIONS) {
    const o = document.createElement("option");
    o.value = opt.value;
    o.textContent = opt.label;
    if (opt.value === migrateLegacyValueFormat(selected)) o.selected = true;
    sel.appendChild(o);
  }
  sel.addEventListener("change", () => {
    appState.liveExprCasts[varIndex] = normalizeValueFormat(sel.value);
    const el = document.getElementById(`live-expr-val-${varIndex}`);
    if (el) {
      el.textContent = formatLiveExpressionValue(
        appState.liveExprValues[varIndex],
        appState.liveExprCasts[varIndex]
      );
    }
  });
  return sel;
}


function ensureVarNames(count) {
  if (!Array.isArray(appState.varNames)) appState.varNames = [];
  while (appState.varNames.length < count) appState.varNames.push("");
  if (appState.varNames.length > count) appState.varNames.length = count;
}

function varNameHintText() {
  ensureVarNames(appState.limits.max_var_slots || 16);
  const parts = [];
  for (let i = 0; i < appState.varNames.length; i++) {
    if (appState.varNames[i]) parts.push(`${appState.varNames[i]}→${i}`);
  }
  return parts.length ? parts.join(", ") : "index or sugar name";
}

function formatVarRefDisplay(ref) {
  if (ref === undefined || ref === null || ref === "") return "";
  if (typeof ref === "number" || (typeof ref === "string" && /^\d+$/.test(ref.trim()))) {
    const idx = Number(ref);
    const name = appState.varNames?.[idx];
    return name ? `${name}` : String(ref);
  }
  return String(ref);
}

function setVarNameFromUi(index, rawName) {
  ensureVarNames(appState.limits.max_var_slots || 16);
  const cleaned = String(rawName || "").trim();
  if (!cleaned) {
    appState.varNames[index] = "";
    setStatus(`v${index} name cleared`);
    return true;
  }
  if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(cleaned)) {
    setStatus(`Invalid var name "${cleaned}": use a Python identifier`);
    return false;
  }
  if (/^v\d+$/.test(cleaned)) {
    setStatus(`Invalid var name ${cleaned}: vN is reserved`);
    return false;
  }
  for (let i = 0; i < appState.varNames.length; i++) {
    if (i !== index && appState.varNames[i] === cleaned) {
      setStatus(`Var name ${cleaned} already used by v${i}`);
      return false;
    }
  }
  appState.varNames[index] = cleaned;
  setStatus(`v${index} named ${cleaned}`);
  return true;
}

/** Resolve int / digit string / vN / sugar name; throws Error on failure. */
function resolveVarRefClient(value) {
  const maxSlots = appState.limits.max_var_slots || 16;
  ensureVarNames(maxSlots);
  if (typeof value === "boolean") {
    throw new Error(`Invalid var reference: ${value}`);
  }
  if (typeof value === "number") {
    if (!Number.isInteger(value) || value < 0 || value >= maxSlots) {
      throw new Error(`var_index must be < ${maxSlots}`);
    }
    return value;
  }
  if (typeof value !== "string") {
    throw new Error(`Invalid var reference type ${typeof value}`);
  }
  const text = value.trim();
  if (!text) {
    throw new Error("Empty var reference");
  }
  if (/^\d+$/.test(text)) {
    const idx = Number(text);
    if (idx >= maxSlots) {
      throw new Error(`var_index must be < ${maxSlots}`);
    }
    return idx;
  }
  const vn = /^v(\d+)$/.exec(text);
  if (vn) {
    const idx = Number(vn[1]);
    if (idx >= maxSlots) {
      throw new Error(`var_index must be < ${maxSlots}`);
    }
    return idx;
  }
  for (let i = 0; i < appState.varNames.length; i++) {
    if (appState.varNames[i] && appState.varNames[i] === text) {
      return i;
    }
  }
  throw new Error(`Unknown var reference '${text}'`);
}

function isVarIndexArgKey(key) {
  return key === "var_index" || /_var_index$/.test(key);
}

/** Returns an error string if any step arg has an unresolvable var reference. */
function validateStepsVarRefs(steps) {
  for (let si = 0; si < (steps || []).length; si++) {
    const step = steps[si] || {};
    const args = step.args || {};
    const command = step.command || "step";
    for (const key of Object.keys(args)) {
      if (!isVarIndexArgKey(key)) continue;
      // Match export/DSL: every *_var_index is resolved, even when use_var is off.
      try {
        resolveVarRefClient(args[key]);
      } catch (err) {
        const detail = err.message || String(err);
        return `step ${si + 1} (${command}) ${key}: ${detail}`;
      }
    }
  }
  return null;
}

/** Returns an error string if any bind/telemetry *_var_index field is unresolvable. */
function validateFieldsVarRefs(fields, label) {
  for (const [key, value] of Object.entries(fields || {})) {
    if (!isVarIndexArgKey(key)) continue;
    try {
      resolveVarRefClient(value);
    } catch (err) {
      const where = label ? `${label} ` : "";
      return `${where}${key}: ${err.message || err}`;
    }
  }
  return null;
}

/** Validate all containers before export / example save. */
function validateGraphVarRefs(graph) {
  for (const c of graph.containers || []) {
    const stepErr = validateStepsVarRefs(c.steps || []);
    if (stepErr) return `${c.label || c.id}: ${stepErr}`;
    const fieldErr = validateFieldsVarRefs(c.fields || {}, c.label || c.type || "binding");
    if (fieldErr) return fieldErr;
  }
  return null;
}

function varNameDatalistId() {
  return "gpc-var-name-datalist";
}

function ensureVarNameDatalist() {
  ensureVarNames(appState.limits.max_var_slots || 16);
  let list = document.getElementById(varNameDatalistId());
  if (!list) {
    list = document.createElement("datalist");
    list.id = varNameDatalistId();
    document.body.appendChild(list);
  }
  const options = [];
  for (let i = 0; i < appState.varNames.length; i++) {
    options.push(`<option value="${i}" label="v${i}"></option>`);
    if (appState.varNames[i]) {
      options.push(
        `<option value="${escapeHtmlAttr(appState.varNames[i])}" label="v${i}"></option>`
      );
    }
  }
  list.innerHTML = options.join("");
  return list.id;
}

function buildVarIndexInputHtml(id, dataAttr, dataValue, currentVal) {
  const shown =
    currentVal === undefined || currentVal === null || currentVal === ""
      ? ""
      : formatVarRefDisplay(currentVal);
  const hint = varNameHintText();
  const listId = ensureVarNameDatalist();
  return `<input id="${id}" ${dataAttr}="${dataValue}" type="text" class="var-index-input" list="${listId}" value="${escapeHtmlAttr(shown)}" placeholder="0 or sugar name" title="${escapeHtmlAttr(hint)}" />`;
}

function renderLiveExpressionGrid() {
  const grid = document.getElementById("live-expr-grid");
  if (!grid) return;
  const count = appState.limits.max_var_slots || 16;
  if (!appState.liveExprValues.length) {
    appState.liveExprValues = Array(count).fill("—");
  }
  ensureLiveExprCasts(count);
  grid.innerHTML = "";
  ensureVarNames(count);
  for (let i = 0; i < count; i++) {
    const cell = document.createElement("div");
    cell.className = "live-expr-cell";
    cell.dataset.varIndex = String(i);
    const idx = document.createElement("span");
    idx.className = "idx";
    idx.textContent = `v${i}`;
    const nameInput = document.createElement("input");
    nameInput.className = "name";
    nameInput.type = "text";
    nameInput.placeholder = "name";
    nameInput.spellcheck = false;
    nameInput.value = appState.varNames[i] || "";
    nameInput.title = "Sugar name for this var slot";
    nameInput.addEventListener("change", () => {
      if (!setVarNameFromUi(i, nameInput.value)) {
        nameInput.value = appState.varNames[i] || "";
      }
    });
    const val = document.createElement("span");
    val.className = "val";
    val.id = `live-expr-val-${i}`;
    val.textContent = formatLiveExpressionValue(appState.liveExprValues[i], appState.liveExprCasts[i]);
    cell.appendChild(idx);
    cell.appendChild(nameInput);
    cell.appendChild(makeLiveExprCastSelect(i, appState.liveExprCasts[i]));
    cell.appendChild(val);
    grid.appendChild(cell);
  }
  updateLiveExpressionStatus();
}

function updateLiveExpressionStatus() {
  const el = document.getElementById("live-expr-status");
  if (!el) return;
  el.classList.remove("active", "stale");
  if (!appState.usbOpen) {
    el.textContent = "USB closed — open a port to stream variables";
    return;
  }
  if (appState.liveExprFresh) {
    el.classList.add("active");
    el.textContent = "Streaming GPC variables (USB, ~2 Hz)";
    return;
  }
  el.classList.add("stale");
  el.textContent = "USB open — waiting for GPC_VARIABLES_TELEMETRY…";
}

function applyLiveExpressionValues(values) {
  const count = appState.limits.max_var_slots || 16;
  for (let i = 0; i < count; i++) {
    appState.liveExprValues[i] = values[i] ?? "—";
  }
  appState.liveExprFresh = true;
  refreshLiveExpressionDisplay();
  updateLiveExpressionStatus();
}

function clearLiveExpressionValues(markStale = true) {
  const count = appState.limits.max_var_slots || 16;
  appState.liveExprValues = Array(count).fill("—");
  appState.liveExprFresh = false;
  refreshLiveExpressionDisplay();
  if (markStale) updateLiveExpressionStatus();
}

/** Parse host log lines containing `gpc_vars v0=<i64> … vN=<i64>`. */
function parseGpcVarsLogLine(text) {
  const marker = text.indexOf("gpc_vars");
  if (marker < 0) return null;
  const segment = text.slice(marker);
  const count = appState.limits.max_var_slots || 16;
  const values = Array(count).fill("—");
  let matched = 0;
  const re = /\bv(\d+)=(-?\d+)\b/g;
  let m;
  while ((m = re.exec(segment)) !== null) {
    const idx = Number(m[1]);
    if (idx >= 0 && idx < count) {
      values[idx] = m[2];
      matched += 1;
    }
  }
  return matched > 0 ? values : null;
}

/**
 * Parse CONTROLLER_STATE_TELEMETRY host log lines:
 * `… CONTROLLER_STATE_TELEMETRY(110) … controller_state=N`
 * Returns enum name string, or null if not a matching line.
 */
function parseControllerStateLogLine(text) {
  if (!text.includes("CONTROLLER_STATE_TELEMETRY")) return null;
  const m = text.match(/\bcontroller_state=(\d+)\b/);
  if (!m) return null;
  const idx = Number(m[1]);
  if (idx < 0 || idx >= CONTROLLER_STATE_BY_VALUE.length) return null;
  return CONTROLLER_STATE_BY_VALUE[idx];
}

/** Apply / clear the LIVE highlight on board cards without a full re-render. */
function applyLiveControllerStateHighlight(stateName) {
  const prev = appState.liveControllerState;
  if (prev === stateName) return;
  appState.liveControllerState = stateName;

  const board = document.getElementById("state-board");
  if (!board) return;

  for (const card of board.querySelectorAll(".state-card[data-state-id]")) {
    const isCurrent = !!stateName && card.dataset.stateId === stateName;
    card.classList.toggle("is-current-state", isCurrent);
    let badge = card.querySelector(".current-state-badge");
    if (isCurrent) {
      if (!badge) {
        badge = document.createElement("div");
        badge.className = "current-state-badge";
        badge.textContent = "LIVE";
        card.appendChild(badge);
      }
    } else if (badge) {
      badge.remove();
    }
  }
}

function clearLiveControllerState() {
  applyLiveControllerStateHighlight(null);
}

function stopLiveExpressionStream() {
  if (liveExprWs) {
    try {
      liveExprWs.onclose = null;
      liveExprWs.onerror = null;
      liveExprWs.onmessage = null;
      liveExprWs.close();
    } catch (_) {
      /* ignore */
    }
    liveExprWs = null;
  }
  appState.liveExprFresh = false;
  clearLiveControllerState();
  updateLiveExpressionStatus();
}

function startLiveExpressionStream() {
  const port = document.getElementById("live-port")?.value || "";
  if (!appState.usbOpen || !port) return;
  stopLiveExpressionStream();

  const proto = location.protocol === "https:" ? "wss:" : "ws:";
  liveExprWs = new WebSocket(`${proto}//${location.host}/ws/usb-log`);
  liveExprWs.onopen = () => {
    liveExprWs.send(JSON.stringify({ port }));
    updateLiveExpressionStatus();
  };
  liveExprWs.onmessage = (ev) => {
    let msg;
    try {
      msg = JSON.parse(ev.data);
    } catch (_) {
      return;
    }
    if (msg.type === "output" && msg.text) {
      const values = parseGpcVarsLogLine(msg.text);
      if (values) applyLiveExpressionValues(values);
      const controllerState = parseControllerStateLogLine(msg.text);
      if (controllerState) applyLiveControllerStateHighlight(controllerState);
    } else if (msg.type === "error" || msg.type === "done") {
      stopLiveExpressionStream();
      if (appState.usbOpen) {
        appState.liveExprFresh = false;
        updateLiveExpressionStatus();
      }
    }
  };
  liveExprWs.onclose = () => {
    liveExprWs = null;
    clearLiveControllerState();
    if (appState.usbOpen) {
      appState.liveExprFresh = false;
      updateLiveExpressionStatus();
    }
  };
  liveExprWs.onerror = () => {
    stopLiveExpressionStream();
  };
}

function renderModalPalette() {
  const panel = document.getElementById("modal-palette");
  panel.innerHTML = "";
  for (const cmd of appState.microCommands) {
    if (PALETTE_SKIP.has(cmd.name)) continue;
    makePaletteItem(cmd.name, cmd, panel);
  }
}

function packBytesLeToUint64(bytes) {
  let value = 0n;
  for (let i = 0; i < bytes.length && i < 8; i++) {
    value |= BigInt(bytes[i] & 0xff) << BigInt(8 * i);
  }
  if (value <= BigInt(Number.MAX_SAFE_INTEGER)) {
    return Number(value);
  }
  return `0x${value.toString(16)}`;
}

/** Parse var_set value: int64, or LE byte list packed into int64. */
function parseVarSetValueInput(text) {
  const s = String(text).trim();
  if (!s) return 0;
  if (s.startsWith("[") || s.includes(",")) {
    const inner = s.startsWith("[") && s.endsWith("]") ? s.slice(1, -1) : s;
    const parts = inner
      .split(",")
      .map((p) => p.trim())
      .filter((p) => p.length);
    if (!parts.length || parts.length > 8) {
      throw new Error("byte list length must be 1..8");
    }
    const bytes = parts.map((p) =>
      p.startsWith("0x") || p.startsWith("0X") ? parseInt(p, 16) : parseInt(p, 10)
    );
    if (bytes.some((b) => Number.isNaN(b) || b < 0 || b > 255)) {
      throw new Error("bytes must be 0..255");
    }
    return Number(packBytesLeToUint64(bytes));
  }
  if (s.startsWith("0x") || s.startsWith("0X")) {
    const n = Number.parseInt(s, 16);
    if (Number.isNaN(n)) throw new Error("invalid hex");
    return n;
  }
  const n = Number.parseInt(s, 10);
  if (Number.isNaN(n)) throw new Error("invalid integer");
  return n;
}

function formatNodeLabel(step) {
  const name = friendlyName(step.command);
  const args = step.args || {};
  if (step.command === "if_condition") {
    const op = args.comparing_type || ">=";
    return `IF ${formatVarRefDisplay(args.first_var_index ?? 0)} ${op} ${formatVarRefDisplay(args.second_var_index ?? 0)}`;
  }
  if (step.command === "end_condition") return "end IF";
  const keys = Object.keys(args).filter((k) => {
    if (k === "reserved") return false;
    // Hide use_var=0 / var_index when not using a var.
    if (k === "use_var" && !isTruthyUseVar(args.use_var)) return false;
    if (k === "var_index" && args.use_var !== undefined && !isTruthyUseVar(args.use_var)) return false;
    return true;
  });
  if (!keys.length) return name;
  return `${name}\n${keys
    .slice(0, 2)
    .map((k) => {
      const v = args[k];
      if (k === "use_var") return isTruthyUseVar(v) ? "use_var" : null;
      if (k.includes("var_index")) return `${k}=${formatVarRefDisplay(v)}`;
      if (fieldUsesHexAddress(step.command, k) && typeof v === "number") {
        return `${k}=${formatHexAddressValue(v)}`;
      }
      if (Array.isArray(v)) return `${k}=[${v.join(", ")}]`;
      if (typeof v === "number" && (step.command === "var_set" || k === "value") && !Number.isInteger(v)) {
        return `${k}=${v}`;
      }
      if (typeof v === "number" && (step.command === "var_set" || k === "value") && v > 255) {
        return `${k}=${v}`;
      }
      return `${k}=${v}`;
    })
    .filter(Boolean)
    .join(", ")}`;
}

function isTruthyUseVar(val) {
  return val === 1 || val === true || val === "1";
}

function commandHasUseVarParam(metaOrParams) {
  const params = Array.isArray(metaOrParams) ? metaOrParams : metaOrParams?.params || [];
  return params.some((p) => p.name === "use_var");
}

function syncUseVarVarIndexVisibility(root, checked) {
  if (!root) return;
  const wrap = root.querySelector("[data-use-var-reveal='var_index']");
  if (wrap) wrap.hidden = !checked;
}

function nodeClassForCommand(command) {
  if (command === "if_condition") return "node-if";
  if (command === "end_condition") return "node-end";
  return "node-step";
}

function cloneSteps(steps) {
  return steps.map(cloneStep);
}

function stepsEqual(a, b) {
  return JSON.stringify(a) === JSON.stringify(b);
}

function resetEditorUndo() {
  editorUndoStack = [];
  editorUndoPending = null;
}

function captureEditorUndoPending() {
  if (editorUndoSuspended || !drawflowEditor) return;
  editorUndoPending = cloneSteps(linearizeDrawflow());
}

function commitEditorUndoIfChanged() {
  if (editorUndoSuspended || !editorUndoPending || !drawflowEditor) {
    editorUndoPending = null;
    return;
  }
  const current = linearizeDrawflow();
  if (!stepsEqual(editorUndoPending, current)) {
    const last = editorUndoStack[editorUndoStack.length - 1];
    if (!last || !stepsEqual(last, editorUndoPending)) {
      editorUndoStack.push(editorUndoPending);
      if (editorUndoStack.length > EDITOR_UNDO_MAX) editorUndoStack.shift();
    }
  }
  editorUndoPending = null;
}

function pushEditorUndoNow() {
  if (editorUndoSuspended || !drawflowEditor) return;
  editorUndoPending = null;
  const steps = cloneSteps(linearizeDrawflow());
  const last = editorUndoStack[editorUndoStack.length - 1];
  if (last && stepsEqual(last, steps)) return;
  editorUndoStack.push(steps);
  if (editorUndoStack.length > EDITOR_UNDO_MAX) editorUndoStack.shift();
}

function undoEditor() {
  if (!editorUndoStack.length) {
    setStatus("Nothing to undo");
    return false;
  }
  editorUndoSuspended = true;
  editorUndoPending = null;
  const steps = editorUndoStack.pop();
  buildDrawflowFromSteps(steps);
  updateModalStepLimit();
  editorUndoSuspended = false;
  setStatus("Undone");
  return true;
}

function buildDrawflowFromSteps(steps, selectStepIndex = null) {
  if (!drawflowEditor) return;
  drawflowEditor.clear();
  clearNodeSelection();

  let prevId = null;
  let x = 60;
  let y = 80;
  const nodeIds = [];
  for (const step of steps) {
    const meta = appState.commandMeta[step.command];
    const inputs = prevId !== null ? 1 : 0;
    const html = `<div class="node-label">${formatNodeLabel(step).replace(/\n/g, "<br>")}</div>`;
    const id = drawflowEditor.addNode(
      step.command,
      inputs,
      1,
      x,
      y,
      nodeClassForCommand(step.command),
      {
        command: step.command,
        args: { ...(step.args || {}) },
        metaName: meta ? meta.name : step.command,
      },
      html
    );
    nodeIds.push(id);
    if (prevId !== null) {
      drawflowEditor.addConnection(prevId, id, "output_1", "input_1");
    }
    prevId = id;
    x += 200;
    if (x > 820) {
      x = 60;
      y += 130;
    }
  }

  if (selectStepIndex != null && nodeIds[selectStepIndex] != null) {
    setNodeSelection([nodeIds[selectStepIndex]], nodeIds[selectStepIndex]);
    if (typeof drawflowEditor.selectNode === "function") {
      drawflowEditor.selectNode(selectedNodeId);
    }
  }
}

function syncNodeSelectionVisuals() {
  document.querySelectorAll("#drawflow .drawflow-node").forEach((el) => {
    const id = el.id.replace(/^node-/, "");
    el.classList.toggle("gpc-selected", selectedNodeIds.has(id));
  });
}

function setNodeSelection(ids, primaryId) {
  selectedNodeIds = new Set(ids.map(String));
  selectedNodeId = primaryId != null ? String(primaryId) : selectedNodeIds.size ? [...selectedNodeIds][0] : null;
  syncNodeSelectionVisuals();
  renderPropsPanel(selectedNodeId);
}

function clearNodeSelection() {
  selectedNodeIds.clear();
  selectedNodeId = null;
  syncNodeSelectionVisuals();
  renderPropsPanel(null);
}

function handleDrawflowNodePointerDown(ev) {
  if (ev.button !== 0) return;
  const nodeEl = ev.target.closest(".drawflow-node");
  if (!nodeEl) {
    if (!ev.ctrlKey && !ev.metaKey) {
      clearNodeSelection();
    }
    return;
  }

  if (!(ev.ctrlKey || ev.metaKey)) return;

  ev.preventDefault();
  ev.stopPropagation();
  skipNextNodeSelected = true;

  const id = nodeEl.id.replace(/^node-/, "");
  const next = new Set(selectedNodeIds);
  if (next.has(id)) next.delete(id);
  else next.add(id);
  setNodeSelection([...next], next.has(id) ? id : [...next][0] ?? null);
}

function collectCopyIndices(steps, selectedIndices) {
  const expanded = new Set();
  for (const index of selectedIndices) {
    for (const idx of expandIfConditionBlock(steps, index)) {
      expanded.add(idx);
    }
  }
  return [...expanded].sort((a, b) => a - b);
}

function getPasteInsertIndex(entries, stepCount) {
  if (!selectedNodeIds.size) return stepCount;
  const idToIndex = new Map(entries.map((entry, index) => [entry.id, index]));
  let maxIndex = -1;
  for (const id of selectedNodeIds) {
    const index = idToIndex.get(String(id));
    if (index !== undefined && index > maxIndex) maxIndex = index;
  }
  return maxIndex >= 0 ? maxIndex + 1 : stepCount;
}

function cloneStep(step) {
  return {
    command: step.command,
    args: JSON.parse(JSON.stringify(step.args || {})),
  };
}

function getOrderedDrawflowEntries() {
  if (!drawflowEditor) return [];
  const exported = drawflowEditor.export();
  const data = exported.drawflow?.Home?.data || {};
  const nodeIds = Object.keys(data);
  if (!nodeIds.length) return [];

  const hasIncoming = new Set();
  for (const id of nodeIds) {
    const node = data[id];
    for (const outputKey of Object.keys(node.outputs || {})) {
      for (const conn of node.outputs[outputKey].connections || []) {
        hasIncoming.add(String(conn.node));
      }
    }
  }

  let startId = nodeIds.find((id) => !hasIncoming.has(id));
  if (!startId) startId = nodeIds[0];

  const entries = [];
  let currentId = startId;
  const visited = new Set();
  while (currentId && !visited.has(currentId)) {
    visited.add(currentId);
    const node = data[currentId];
    const command = node.data?.command;
    if (command) {
      entries.push({
        id: String(currentId),
        command,
        args: { ...(node.data.args || {}) },
      });
    }
    let nextId = null;
    for (const outputKey of Object.keys(node.outputs || {})) {
      const conns = node.outputs[outputKey].connections || [];
      if (conns.length > 0) {
        nextId = String(conns[0].node);
        break;
      }
    }
    currentId = nextId;
  }
  return entries;
}

function expandIfConditionBlock(steps, startIndex) {
  if (steps[startIndex]?.command !== "if_condition") return [startIndex];
  let depth = 0;
  for (let j = startIndex; j < steps.length; j++) {
    if (steps[j].command === "if_condition") depth++;
    if (steps[j].command === "end_condition") {
      depth--;
      if (depth === 0) {
        const indices = [];
        for (let k = startIndex; k <= j; k++) indices.push(k);
        return indices;
      }
    }
  }
  return [startIndex];
}

function copySelectedCommand() {
  if (!drawflowEditor || !selectedNodeIds.size) {
    setStatus("Select a command to copy");
    return false;
  }

  const entries = getOrderedDrawflowEntries();
  const idToIndex = new Map(entries.map((entry, index) => [entry.id, index]));
  const indices = [...selectedNodeIds]
    .map((id) => idToIndex.get(String(id)))
    .filter((index) => index !== undefined)
    .sort((a, b) => a - b);
  if (!indices.length) {
    setStatus("Select a command to copy");
    return false;
  }

  const steps = entries.map(({ command, args }) => ({ command, args }));
  const copyIndices = collectCopyIndices(steps, indices);
  commandClipboard = copyIndices.map((i) => cloneStep(steps[i]));

  const label = commandClipboard.length === 1 ? "command" : "commands";
  setStatus(`Copied ${commandClipboard.length} ${label}`);
  return true;
}

function pasteCommands() {
  if (!drawflowEditor) return false;
  if (!commandClipboard.length) {
    setStatus("Nothing to paste — copy a command first");
    return false;
  }

  const steps = linearizeDrawflow();
  const entries = getOrderedDrawflowEntries();
  const insertIndex = getPasteInsertIndex(entries, steps.length);

  const pasted = commandClipboard.map(cloneStep);
  const newSteps = [...steps.slice(0, insertIndex), ...pasted, ...steps.slice(insertIndex)];
  if (newSteps.length > appState.limits.max_steps) {
    setStatus(`Cannot paste: would exceed max ${appState.limits.max_steps} steps`);
    updateModalStepLimit();
    return false;
  }

  pushEditorUndoNow();
  buildDrawflowFromSteps(newSteps, insertIndex);
  updateModalStepLimit();
  const label = pasted.length === 1 ? "command" : "commands";
  setStatus(`Pasted ${pasted.length} ${label}`);
  return true;
}

function linearizeDrawflow() {
  return getOrderedDrawflowEntries().map(({ command, args }) => ({ command, args }));
}

function addNodeAtDrop(command, args, clientX, clientY) {
  if (!drawflowEditor) return;
  if (countDrawflowNodes() >= appState.limits.max_steps) {
    setStatus(`No room for more steps (max ${appState.limits.max_steps})`);
    updateModalStepLimit();
    return;
  }
  const precanvas = drawflowEditor.precanvas;
  const rect = precanvas.getBoundingClientRect();
  const zoom = drawflowEditor.zoom || 1;
  const x = (clientX - rect.left) / zoom;
  const y = (clientY - rect.top) / zoom;
  const meta = appState.commandMeta[command];
  const step = { command, args: args || defaultArgs(meta) };
  const html = `<div class="node-label">${formatNodeLabel(step).replace(/\n/g, "<br>")}</div>`;
  pushEditorUndoNow();
  drawflowEditor.addNode(
    command,
    1,
    1,
    x,
    y,
    nodeClassForCommand(command),
    { command, args: { ...step.args }, metaName: command },
    html
  );
  updateModalStepLimit();
}

function countDrawflowNodes() {
  if (!drawflowEditor) return 0;
  const exported = drawflowEditor.export();
  return Object.keys(exported.drawflow?.Home?.data || {}).length;
}

function updateModalStepLimit() {
  const used = countDrawflowNodes();
  setLimitHint(document.getElementById("modal-limit"), used, appState.limits.max_steps, "steps");
}

function renderPropsPanel(nodeId) {
  const panel = document.getElementById("props-panel");
  const selectionCount = selectedNodeIds.size;

  if (!nodeId || !drawflowEditor) {
    panel.innerHTML = `<h3>Properties</h3><p class="props-empty">Select a node to edit parameters.</p>`;
    return;
  }

  if (selectionCount > 1) {
    panel.innerHTML = `<h3>${selectionCount} commands selected</h3><p class="props-empty">Ctrl+click to change selection. Ctrl+C to copy.</p>`;
    return;
  }

  const node = drawflowEditor.getNodeFromId(nodeId);
  if (!node) return;
  const command = node.data.command;
  const meta = appState.commandMeta[command];
  if (!meta || !meta.params || !meta.params.length) {
    panel.innerHTML = `<h3>Properties</h3><p class="props-empty">${friendlyName(command)} — no parameters</p>`;
    return;
  }
  if (!node.data.argFormats) node.data.argFormats = {};

  const hasUseVar = commandHasUseVarParam(meta);
  const useVarOn = isTruthyUseVar(node.data.args.use_var);

  const fields = meta.params
    .map((p) => {
      const val = node.data.args[p.name];
      if (p.name === "use_var") {
        const checked = isTruthyUseVar(val);
        return `
          <div class="field field-checkbox">
            <label for="prop-use_var" class="checkbox-label">
              <input id="prop-use_var" data-param="use_var" type="checkbox"${checked ? " checked" : ""} />
              use var
            </label>
          </div>`;
      }
      if (p.name === "var_index" && hasUseVar) {
        const maxValue = p.max_value ?? (appState.limits.max_var_slots - 1);
        const hintText = p.hint || `0–${maxValue}`;
        return `
          <div class="field" data-use-var-reveal="var_index"${useVarOn ? "" : " hidden"}>
            <label for="prop-var_index">${p.name}<span class="limit-hint">${varNameHintText()}</span></label>
            ${buildVarIndexInputHtml("prop-var_index", "data-param", "var_index", val ?? 0)}
          </div>`;
      }
      if (/(_)?var_index$/.test(p.name) || p.name.endsWith("_var_index") || ["var_index","first_var_index","second_var_index","dest_var_index","src_var_index"].includes(p.name)) {
        if (!(p.name === "var_index" && hasUseVar)) {
          return `
          <div class="field">
            <label for="prop-${p.name}">${p.name}<span class="limit-hint">${varNameHintText()}</span></label>
            ${buildVarIndexInputHtml("prop-" + p.name, "data-param", p.name, val ?? 0)}
          </div>`;
        }
      }
      const usesFormat = fieldUsesValueFormat(command, p);
      const argFormat = getNodeArgFormat(node, p.name);
      const displayVal = usesFormat
        ? formatArgForDisplay(val, argFormat, { isList: isListParam(p), isVarSetValue: !!p.accepts_byte_list })
        : Array.isArray(val)
          ? val.join(", ")
          : val ?? "";
      if (isListParam(p)) {
        const maxLen = p.max_len || appState.limits.comm_data_length;
        return `
          <div class="field">
            <label for="prop-${p.name}">${p.name}<span class="limit-hint">max ${maxLen} bytes</span></label>
            <div class="field-format-row">
              <select id="prop-format-${p.name}" data-format-for="${p.name}" class="value-format-select" title="Value representation">
                ${VALUE_FORMAT_OPTIONS.map(
                  (opt) =>
                    `<option value="${opt.value}"${opt.value === argFormat ? " selected" : ""}>${opt.label}</option>`
                ).join("")}
              </select>
              <input id="prop-${p.name}" data-param="${p.name}" data-is-list="1" data-max-len="${maxLen}" class="wide"
                value="${escapeHtmlAttr(displayVal)}" placeholder="72, 69, 76 or 0x48, 0x45 or &quot;HELLO&quot;" />
            </div>
          </div>`;
      }
      if (p.accepts_byte_list) {
        const maxLen = p.max_len || appState.limits.comm_data_length || 8;
        return `
          <div class="field">
            <label for="prop-${p.name}">${p.name}<span class="limit-hint">int64 or LE bytes (max ${maxLen})</span></label>
            <div class="field-format-row">
              <select id="prop-format-${p.name}" data-format-for="${p.name}" class="value-format-select" title="Value representation">
                ${VALUE_FORMAT_OPTIONS.map(
                  (opt) =>
                    `<option value="${opt.value}"${opt.value === argFormat ? " selected" : ""}>${opt.label}</option>`
                ).join("")}
              </select>
              <input id="prop-${p.name}" data-param="${p.name}" data-accepts-byte-list="1" data-max-len="${maxLen}" class="wide"
                value="${escapeHtmlAttr(displayVal)}" placeholder="3500, 0xDAC, 3.14, &quot;text&quot;, or 1, 2, 3" />
            </div>
          </div>`;
      }
      if (p.name === "comparing_type") {
        const ops = [">=", ">", "<=", "<", "==", "!="];
        const opts = ops
          .map((op) => `<option value="${op}"${displayVal === op ? " selected" : ""}>${op}</option>`)
          .join("");
        return `
          <div class="field">
            <label for="prop-${p.name}">${p.name}</label>
            <select id="prop-${p.name}" data-param="${p.name}">${opts}</select>
          </div>`;
      }
      if (fieldUsesHexAddress(command, p.name)) {
        const shown = formatHexAddressValue(val);
        return `
          <div class="field">
            <label for="prop-${p.name}">${p.name}<span class="limit-hint">hex (0x)</span></label>
            <input id="prop-${p.name}" data-param="${p.name}" data-hex-address="1" value="${escapeHtmlAttr(shown)}" placeholder="0x12" />
          </div>`;
      }
      const maxValue = p.max_value;
      const maxAttr = maxValue ? ` max="${maxValue}"` : "";
      const hintText = p.hint || (maxValue ? `max ${maxValue}` : "");
      const hintLabel = hintText
        ? `<span class="limit-hint">${hintText}</span>`
        : "";
      return `
        <div class="field">
          <label for="prop-${p.name}">${p.name}${hintLabel}</label>
          <input id="prop-${p.name}" data-param="${p.name}" value="${displayVal}"${maxAttr} />
        </div>`;
    })
    .join("");

  panel.innerHTML = `<h3>${friendlyName(command)}</h3>${fields}`;

  panel.querySelectorAll("[data-format-for]").forEach((sel) => {
    sel.addEventListener("change", () => {
      const param = sel.dataset.formatFor;
      const format = normalizeValueFormat(sel.value);
      node.data.argFormats[param] = format;
      drawflowEditor.updateNodeDataFromId(nodeId, node.data);
      const paramMeta = meta.params.find((p) => p.name === param);
      const input = panel.querySelector(`#prop-${param}`);
      if (!input || !paramMeta) return;
      input.value = formatArgForDisplay(node.data.args[param], format, {
        isList: isListParam(paramMeta),
        isVarSetValue: !!paramMeta.accepts_byte_list,
      });
    });
  });

  panel.querySelectorAll("[data-param]").forEach((el) => {
    const handler = () => {
      const param = el.dataset.param;
      const formatSel = panel.querySelector(`#prop-format-${param}`);
      const format = formatSel ? normalizeValueFormat(formatSel.value) : "int";
      let value;
      if (el.type === "checkbox") {
        value = el.checked ? 1 : 0;
        if (param === "use_var") syncUseVarVarIndexVisibility(panel, el.checked);
      } else if (el.dataset.isList === "1") {
        const maxLen = parseInt(el.dataset.maxLen || "8", 10);
        try {
          value = parseBytesArrayWithFormat(el.value, format, maxLen);
        } catch (err) {
          setStatus(`Invalid ${param}: ${err.message || err}`);
          return;
        }
        if (value.length > maxLen) {
          value = value.slice(0, maxLen);
          el.value = formatBytesArrayForDisplay(value, format);
          setStatus(`Trimmed ${param} to max ${maxLen} bytes`);
        }
      } else if (el.dataset.acceptsByteList === "1") {
        try {
          value = parseInt64WithFormat(el.value, format, { allowByteList: formatAllowsByteList(format) });
        } catch (err) {
          setStatus(`Invalid ${param}: ${err.message || err}`);
          return;
        }
      } else if (el.dataset.hexAddress === "1") {
        try {
          value = parseHexAddressInput(el.value);
          el.value = formatHexAddressValue(value);
        } catch (err) {
          setStatus(`Invalid ${param}: ${err.message || err}`);
          return;
        }
      } else if (el.tagName === "SELECT") {
        value = el.value;
      } else if (el.value.startsWith("0x")) {
        value = parseInt(el.value, 16);
      } else if (el.value.includes(".")) {
        value = parseFloat(el.value);
      } else {
        value = parseInt(el.value, 10);
        if (Number.isNaN(value)) value = el.value;
        if (el.max !== "" && typeof value === "number" && value > Number(el.max)) {
          value = Number(el.max);
          el.value = String(value);
          setStatus(`${param} capped at ${el.max}`);
        }
      }
      // getNodeFromId returns a deep clone — write back via updateNodeDataFromId
      // so Save / linearizeDrawflow see the edited args instead of defaults.
      node.data.args[param] = value;
      drawflowEditor.updateNodeDataFromId(nodeId, node.data);
      commitEditorUndoIfChanged();
      const label = formatNodeLabel({ command, args: node.data.args });
      const nodeEl = document.getElementById(`node-${nodeId}`);
      const inner = nodeEl?.querySelector(".node-label");
      if (inner) inner.innerHTML = label.replace(/\n/g, "<br>");
    };
    el.addEventListener("input", handler);
    el.addEventListener("change", handler);
    el.addEventListener("focusin", captureEditorUndoPending);
  });
}

function captureDrawflowUndoPending(ev) {
  if (editorUndoSuspended || ev.button !== 0) return;
  if (ev.ctrlKey || ev.metaKey) return;
  const target = ev.target;
  if (
    target.closest(".drawflow-node") ||
    target.classList.contains("input") ||
    target.classList.contains("output") ||
    target.classList.contains("main-path")
  ) {
    captureEditorUndoPending();
  }
}

function initDrawflow() {
  const container = document.getElementById("drawflow");
  drawflowEditor = new Drawflow(container);
  drawflowEditor.reroute = true;
  drawflowEditor.start();

  container.addEventListener("dragover", (ev) => {
    ev.preventDefault();
    ev.dataTransfer.dropEffect = "copy";
  });
  container.addEventListener("mousedown", handleDrawflowNodePointerDown, true);
  container.addEventListener("mousedown", captureDrawflowUndoPending, true);
  container.addEventListener("drop", (ev) => {
    ev.preventDefault();
    const raw = ev.dataTransfer.getData("application/gpc-command");
    if (!raw) return;
    const payload = JSON.parse(raw);
    addNodeAtDrop(payload.command, payload.args, ev.clientX, ev.clientY);
  });

  drawflowEditor.on("nodeSelected", (id) => {
    if (skipNextNodeSelected) {
      skipNextNodeSelected = false;
      return;
    }
    setNodeSelection([id], id);
  });
  drawflowEditor.on("nodeUnselected", () => {
    if (selectedNodeIds.size <= 1) {
      clearNodeSelection();
    }
  });
  drawflowEditor.on("nodeCreated", () => {
    commitEditorUndoIfChanged();
    updateModalStepLimit();
  });
  drawflowEditor.on("nodeRemoved", () => {
    commitEditorUndoIfChanged();
    updateModalStepLimit();
  });
  drawflowEditor.on("connectionCreated", commitEditorUndoIfChanged);
  drawflowEditor.on("connectionRemoved", commitEditorUndoIfChanged);
}

function openEditor(containerId) {
  const container = appState.containers[containerId];
  if (!container) return;
  if (container.type === "telemetry") {
    openTelemetryEditor(containerId);
    return;
  }

  clearEditorModalError();
  appState.activeContainerId = containerId;
  document.getElementById("modal-title").textContent = `${container.label} — sequence editor`;
  setEditorModalActions(container);
  document.getElementById("editor-modal").classList.add("open");
  renderModalPalette();

  if (!drawflowEditor) initDrawflow();
  else drawflowEditor.clear();

  resetEditorUndo();
  buildDrawflowFromSteps(container.steps || []);
  updateModalStepLimit();
}

function closeEditor(save) {
  if (save && appState.activeContainerId) {
    const steps = linearizeDrawflow();
    if (steps.length > appState.limits.max_steps) {
      const msg = `Cannot save: ${steps.length} steps, max is ${appState.limits.max_steps}`;
      showEditorModalError(msg);
      setStatus(msg);
      updateModalStepLimit();
      return;
    }
    const varErr = validateStepsVarRefs(steps);
    if (varErr) {
      showEditorModalError(varErr);
      setStatus(`Cannot save: ${varErr}`);
      return;
    }
    clearEditorModalError();
    const container = appState.containers[appState.activeContainerId];
    if (container) container.steps = steps;
    renderBoard();
    setStatus(`Saved ${container.label} (${steps.length} steps)`);
  }
  clearEditorModalError();
  appState.activeContainerId = null;
  setEditorModalActions(null);
  resetEditorUndo();
  document.getElementById("editor-modal").classList.remove("open");
  const modalLimit = document.getElementById("modal-limit");
  if (modalLimit) modalLimit.hidden = true;
}

function openTelemetryEditor(containerId) {
  clearBindModalError();
  const container = appState.containers[containerId];
  if (!container) return;
  appState.bindMode = "telemetry-edit";
  appState.activeContainerId = containerId;
  document.getElementById("bind-modal-title").textContent = `Edit telemetry: ${container.label}`;
  setLimitHint(
    document.getElementById("bind-modal-limit"),
    Object.keys(container.fields || {}).length,
    appState.limits.max_telemetry_fields,
    "fields"
  );
  renderBindForm(container);
  setBindModalActions("telemetry-edit");
  document.getElementById("bind-modal").classList.add("open");
}

function openCommandBindingEditor(containerId) {
  clearBindModalError();
  const container = appState.containers[containerId];
  if (!container || container.type !== "command") return;
  // Leave sequence editor open underneath if we came from it; bind modal sits on top.
  appState.bindMode = "command-edit";
  appState.activeContainerId = containerId;
  document.getElementById("bind-modal-title").textContent = `Edit command binding: ${container.trigger || container.label}`;
  setLimitHint(
    document.getElementById("bind-modal-limit"),
    countByType("command"),
    appState.limits.max_command_bindings,
    "bindings"
  );
  renderBindForm(container);
  setBindModalActions("command-edit");
  document.getElementById("bind-modal").classList.add("open");
}

function collectBindFormFields() {
  const fields = {};
  document.querySelectorAll("#bind-form [data-field]").forEach((el) => {
    const key = el.dataset.field;
    let val = el.value;
    if (el.dataset.isEnum === "1") {
      fields[key] = val;
      return;
    }
    if (el.type === "number") {
      val = parseInt(val, 10) || 0;
    } else if (val === "true" || val === "false") {
      val = val === "true";
    } else if (/^\d+$/.test(val)) {
      val = parseInt(val, 10);
    }
    fields[key] = val;
  });
  return fields;
}

function validateCommandBindingFields(trigger, fields) {
  const triggerDef =
    appState.bluelinkCommands.find((t) => (t.payload_type || t.name) === trigger) ||
    appState.bluelinkCommands[0];
  const enumFieldNames = new Set(
    (triggerDef?.fields || []).filter(isBindCommandEnumField).map((f) => f.name)
  );
  for (const key of Object.keys(fields)) {
    if (!key.endsWith("_var_index")) continue;
    const fieldName = key.slice(0, -"_var_index".length);
    if (enumFieldNames.has(fieldName)) {
      return (
        `Field '${fieldName}' is an enum and must be a match value. ` +
        "Only non-enum fields can use 'store to var' (index or sugar name)."
      );
    }
  }
  const hasMatchField = Object.keys(fields).some((key) => !key.endsWith("_var_index"));
  if (!hasMatchField) {
    return (
      "Cannot save this command binding: every field uses 'store to var'. " +
      "Uncheck that box on at least one field (for example brake_mode) and set a match value " +
      "so the GPC knows when to run this sequence."
    );
  }
  return null;
}

function openBindModal(mode) {
  clearBindModalError();
  appState.bindMode = mode;
  appState.activeContainerId = null;
  document.getElementById("bind-modal-title").textContent =
    mode === "command" ? "Add COMMAND binding" : "Add TELEMETRY binding";
  if (mode === "command") {
    setLimitHint(
      document.getElementById("bind-modal-limit"),
      countByType("command"),
      appState.limits.max_command_bindings,
      "bindings"
    );
  } else {
    setLimitHint(
      document.getElementById("bind-modal-limit"),
      countByType("telemetry"),
      appState.limits.max_telemetry_bindings,
      "bindings"
    );
  }
  renderBindForm(null);
  setBindModalActions(mode);
  document.getElementById("bind-modal").classList.add("open");
}

function renderBindForm(existing) {
  clearBindModalError();
  const form = document.getElementById("bind-form");
  const isCommand =
    appState.bindMode === "command" ||
    appState.bindMode === "command-edit" ||
    (existing && existing.type === "command");
  const isTelemetry = !isCommand;

  const triggers = isCommand
    ? appState.bluelinkCommands
    : appState.telemetryStructs.map((t) => ({
        payload_type: t.payload_type,
        struct_name: t.struct_name,
        fields: t.fields,
      }));

  const selectedTrigger = existing?.trigger || "";
  const triggerOptions = triggers
    .map((t) => {
      const pt = t.payload_type || t.name;
      return `<option value="${pt}"${pt === selectedTrigger ? " selected" : ""}>${pt}</option>`;
    })
    .join("");

  let extraFields = "";
  if (isTelemetry) {
    extraFields += `
      <div class="field">
        <label for="bind-rate">rate (Hz)</label>
        <input id="bind-rate" type="number" min="1" value="${existing?.rate || 1}" />
      </div>`;
  }

  const triggerDef = triggers.find((t) => (t.payload_type || t.name) === selectedTrigger) || triggers[0];
  if (triggerDef) {
    const fields = triggerDef.fields || [];
    for (const f of fields) {
      const varIndexKey = `${f.name}_var_index`;
      if (isTelemetry) {
        const val = existing?.fields?.[varIndexKey] ?? 0;
        extraFields += `
          <div class="field">
            <label for="bind-${varIndexKey}">${f.name} → var<span class="limit-hint">${escapeHtmlAttr(varNameHintText())}</span></label>
            ${buildVarIndexInputHtml(`bind-${varIndexKey}`, "data-field", varIndexKey, val)}
          </div>`;
      } else {
        extraFields += buildBindCommandFieldRowHtml(f, existing?.fields || {});
      }
    }
  }

  form.innerHTML = `
    <div class="field">
      <label for="bind-trigger">Trigger</label>
      <select id="bind-trigger">${triggerOptions}</select>
    </div>
    ${extraFields}
  `;

  document.getElementById("bind-trigger").addEventListener("change", () => {
    clearBindModalError();
    const trig = document.getElementById("bind-trigger").value;
    const partial = existing ? { ...existing, trigger: trig, fields: {} } : null;
    if (partial) partial.trigger = trig;
    else {
      renderBindForm(isCommand ? { type: "command", trigger: trig, fields: {} } : { type: "telemetry", trigger: trig, rate: 1, fields: {} });
      return;
    }
    renderBindForm(partial);
  });

  if (isCommand && triggerDef) {
    initBindCommandFieldToggles(triggerDef);
  }
}

function closeBindModal(save) {
  if (save) {
    const trigger = document.getElementById("bind-trigger")?.value;
    const fields = collectBindFormFields();

    const varFieldErr = validateFieldsVarRefs(fields, trigger || "binding");
    if (varFieldErr) {
      showBindModalError(varFieldErr);
      setStatus(`Cannot save: ${varFieldErr}`);
      return;
    }

    if (appState.bindMode === "telemetry-edit" && appState.activeContainerId) {
      const fieldCount = Object.keys(fields).length;
      if (fieldCount > appState.limits.max_telemetry_fields) {
        showBindModalError(
          `Too many telemetry fields (${fieldCount}). Maximum is ${appState.limits.max_telemetry_fields}.`
        );
        return;
      }
      const c = appState.containers[appState.activeContainerId];
      c.trigger = trigger;
      c.rate = parseInt(document.getElementById("bind-rate")?.value || "1", 10);
      c.fields = fields;
      c.label = `TELEMETRY: ${trigger}`;
      renderBoard();
      setStatus(`Updated telemetry binding: ${c.label}`);
    } else if (appState.bindMode === "command-edit" && appState.activeContainerId) {
      const error = validateCommandBindingFields(trigger, fields);
      if (error) {
        showBindModalError(error);
        return;
      }
      const c = appState.containers[appState.activeContainerId];
      c.trigger = trigger;
      c.fields = fields;
      c.label = formatCommandBindingLabel(trigger, fields);
      // Keep existing steps; only binding match params change.
      renderBoard();
      const editorOpen = document.getElementById("editor-modal")?.classList.contains("open");
      if (editorOpen) {
        document.getElementById("modal-title").textContent = `${c.label} — sequence editor`;
      }
      setStatus(`Updated command binding: ${c.label}`);
    } else if (appState.bindMode === "command") {
      const error = validateCommandBindingFields(trigger, fields);
      if (error) {
        showBindModalError(error);
        return;
      }
      if (countByType("command") >= appState.limits.max_command_bindings) {
        showBindModalError(
          `No room for more command bindings (maximum is ${appState.limits.max_command_bindings}).`
        );
        return;
      }
      const id = `command_${appState.commandCounter++}`;
      appState.containers[id] = {
        id,
        type: "command",
        label: formatCommandBindingLabel(trigger, fields),
        trigger,
        fields,
        steps: [],
      };
      renderBoard();
      setStatus("Binding added");
    } else if (appState.bindMode === "telemetry") {
      if (countByType("telemetry") >= appState.limits.max_telemetry_bindings) {
        showBindModalError(
          `No room for more telemetry bindings (maximum is ${appState.limits.max_telemetry_bindings}).`
        );
        return;
      }
      const fieldCount = Object.keys(fields).length;
      if (fieldCount > appState.limits.max_telemetry_fields) {
        showBindModalError(
          `Too many telemetry fields (${fieldCount}). Maximum is ${appState.limits.max_telemetry_fields}.`
        );
        return;
      }
      const id = `telemetry_${appState.telemetryCounter++}`;
      appState.containers[id] = {
        id,
        type: "telemetry",
        label: `TELEMETRY: ${trigger}`,
        trigger,
        rate: parseInt(document.getElementById("bind-rate")?.value || "1", 10),
        fields,
        steps: [],
      };
      renderBoard();
      setStatus("Binding added");
    }
  }

  const editingCommandFromEditor =
    appState.bindMode === "command-edit" &&
    document.getElementById("editor-modal")?.classList.contains("open");
  const restoreContainerId = editingCommandFromEditor ? appState.activeContainerId : null;

  clearBindModalError();
  appState.bindMode = null;
  setBindModalActions(null);
  document.getElementById("bind-modal").classList.remove("open");
  const bindLimit = document.getElementById("bind-modal-limit");
  if (bindLimit) bindLimit.hidden = true;

  // If we edited binding params while the sequence editor was open, keep that container active.
  if (restoreContainerId) {
    appState.activeContainerId = restoreContainerId;
  }
}

function buildGraphPayload() {
  ensureFixedContainers();
  const containers = [];

  for (const slot of FIXED_SLOTS) {
    const c = appState.containers[slot.id];
    if (!c || !(c.steps || []).length) continue;
    const entry = {
      id: c.id,
      type: c.type,
      label: c.label,
      steps: c.steps,
    };
    if (c.state) entry.state = c.state;
    containers.push(entry);
  }

  for (const c of Object.values(appState.containers)) {
    if (c.type === "command" && c.trigger && (c.steps || []).length) {
      containers.push({
        id: c.id,
        type: "command",
        label: c.label,
        trigger: c.trigger,
        fields: c.fields || {},
        steps: c.steps,
      });
    }
    if (c.type === "telemetry" && c.trigger && c.rate) {
      containers.push({
        id: c.id,
        type: "telemetry",
        label: c.label,
        trigger: c.trigger,
        rate: c.rate,
        fields: c.fields || {},
        steps: [],
      });
    }
  }

  ensureVarNames(appState.limits.max_var_slots || 16);
  ensureLiveExprCasts(appState.limits.max_var_slots || 16);
  return {
    config: appState.config,
    var_names: appState.varNames.slice(),
    live_expr_casts: appState.liveExprCasts.slice(),
    containers,
  };
}

function applyLoadedGraph(graph) {
  appState.config = graph.config || appState.config;
  const count = appState.limits.max_var_slots || 16;
  ensureVarNames(count);
  ensureLiveExprCasts(count);
  const incoming = Array.isArray(graph.var_names) ? graph.var_names : [];
  for (let i = 0; i < count; i++) {
    appState.varNames[i] = incoming[i] != null ? String(incoming[i]).trim() : "";
  }
  const incomingCasts = Array.isArray(graph.live_expr_casts) ? graph.live_expr_casts : [];
  for (let i = 0; i < count; i++) {
    appState.liveExprCasts[i] = migrateLegacyValueFormat(
      incomingCasts[i] != null ? incomingCasts[i] : "int"
    );
  }
  appState.containers = {};
  appState.commandCounter = 0;
  appState.telemetryCounter = 0;
  ensureFixedContainers();
  renderConfigControls();

  for (const c of graph.containers || []) {
    if (c.type === "command") {
      const id = c.id || `command_${appState.commandCounter++}`;
      const fields = c.fields || {};
      const trigger = c.trigger || "";
      appState.containers[id] = {
        ...c,
        id,
        steps: c.steps || [],
        label: formatCommandBindingLabel(trigger, fields),
      };
      const n = parseInt(id.replace("command_", ""), 10);
      if (!Number.isNaN(n)) appState.commandCounter = Math.max(appState.commandCounter, n + 1);
    } else if (c.type === "telemetry") {
      const id = c.id || `telemetry_${appState.telemetryCounter++}`;
      appState.containers[id] = { ...c, id, steps: [] };
      const n = parseInt(id.replace("telemetry_", ""), 10);
      if (!Number.isNaN(n)) appState.telemetryCounter = Math.max(appState.telemetryCounter, n + 1);
    } else if (appState.containers[c.id]) {
      appState.containers[c.id] = {
        ...appState.containers[c.id],
        steps: c.steps || [],
      };
    } else {
      appState.containers[c.id] = { ...c, steps: c.steps || [] };
    }
  }
  renderBoard();
  renderLiveExpressionGrid();
}

function setLoadedExample(name) {
  appState.loadedExample = name || null;
}

async function loadGraph(exampleName) {
  const isExample = typeof exampleName === "string" && exampleName.length > 0;
  setStatus(isExample ? `Loading example ${exampleName}…` : "Loading config…");
  const url = isExample
    ? `/api/graph/load?example=${encodeURIComponent(exampleName)}`
    : "/api/graph/load";
  const res = await fetch(url);
  const data = await res.json();
  if (!data.ok) {
    setStatus(`Load failed: ${data.error}`);
    return;
  }
  applyLoadedGraph(data.graph);
  setLoadedExample(isExample ? exampleName : null);
  if (isExample) {
    setStatus(`Loaded example ${exampleName}`);
  } else {
    setStatus(`Loaded ${(data.graph.containers || []).length} container(s)`);
  }
}

async function saveToExample(name) {
  syncConfigFromControls();
  const graph = buildGraphPayload();
  const varErr = validateGraphVarRefs(graph);
  if (varErr) {
    setStatus(`Cannot save example: ${varErr}`);
    return;
  }
  setStatus(`Saving to example ${name}…`);
  const res = await fetch("/api/graph/examples/save", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ graph, example: name }),
  });
  const data = await res.json();
  if (!data.ok) {
    setStatus(`Save to example failed: ${data.error}`);
    return;
  }
  setLoadedExample(name);
  setStatus(`Saved to example ${name}`);
}

let pendingSaveExampleName = null;

function openSaveExampleConfirm(name) {
  pendingSaveExampleName = name;
  const label = document.getElementById("confirm-save-example-name");
  if (label) label.textContent = name;
  document.getElementById("confirm-save-example-modal")?.classList.add("open");
}

function closeSaveExampleConfirm() {
  pendingSaveExampleName = null;
  document.getElementById("confirm-save-example-modal")?.classList.remove("open");
}

async function confirmSaveToExample() {
  const name = pendingSaveExampleName;
  closeSaveExampleConfirm();
  if (!name) return;
  await saveToExample(name);
}

function closeMenu(menuId, btnId) {
  const menu = document.getElementById(menuId);
  const btn = document.getElementById(btnId);
  if (!menu || !btn) return;
  menu.classList.remove("open");
  menu.hidden = true;
  btn.setAttribute("aria-expanded", "false");
}

function openMenu(menuId, btnId) {
  const menu = document.getElementById(menuId);
  const btn = document.getElementById(btnId);
  if (!menu || !btn) return;
  menu.hidden = false;
  menu.classList.add("open");
  btn.setAttribute("aria-expanded", "true");
}

function closeExamplesMenu() {
  closeMenu("examples-menu", "btn-examples");
}

function closeSaveExampleMenu() {
  closeMenu("save-example-menu", "btn-save-example");
}

function closeAllExampleMenus() {
  closeExamplesMenu();
  closeSaveExampleMenu();
}

async function fetchExampleNames() {
  const res = await fetch("/api/graph/examples");
  const data = await res.json();
  if (!data.ok) {
    throw new Error(data.error || "Failed to list examples");
  }
  return data.examples || [];
}

function fillExamplesMenu(menu, examples, onPick) {
  menu.innerHTML = "";
  if (!examples.length) {
    const empty = document.createElement("div");
    empty.className = "empty";
    empty.textContent = "No examples found";
    menu.appendChild(empty);
    return;
  }
  for (const name of examples) {
    const item = document.createElement("button");
    item.type = "button";
    item.role = "option";
    item.textContent = name;
    item.addEventListener("click", async () => {
      closeAllExampleMenus();
      await onPick(name);
    });
    menu.appendChild(item);
  }
}

async function toggleExamplesMenu() {
  const menu = document.getElementById("examples-menu");
  if (!menu) return;
  if (menu.classList.contains("open")) {
    closeExamplesMenu();
    return;
  }
  closeSaveExampleMenu();
  setStatus("Loading examples…");
  try {
    const examples = await fetchExampleNames();
    fillExamplesMenu(menu, examples, loadGraph);
    openMenu("examples-menu", "btn-examples");
    setStatus("Ready");
  } catch (err) {
    setStatus(`Examples failed: ${err.message}`);
  }
}

async function toggleSaveExampleMenu() {
  const menu = document.getElementById("save-example-menu");
  if (!menu) return;
  if (menu.classList.contains("open")) {
    closeSaveExampleMenu();
    return;
  }
  closeExamplesMenu();
  setStatus("Loading examples…");
  try {
    const examples = await fetchExampleNames();
    fillExamplesMenu(menu, examples, openSaveExampleConfirm);
    openMenu("save-example-menu", "btn-save-example");
    setStatus("Ready");
  } catch (err) {
    setStatus(`Examples failed: ${err.message}`);
  }
}

async function exportGraph() {
  syncConfigFromControls();
  const graph = buildGraphPayload();
  if (!graph.containers.length) {
    setStatus("Nothing to export — add at least one sequence or binding");
    return;
  }
  const varErr = validateGraphVarRefs(graph);
  if (varErr) {
    setStatus(`Export failed: ${varErr}`);
    return;
  }
  setStatus("Exporting…");
  const res = await fetch("/api/graph/export", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ graph }),
  });
  const data = await res.json();
  if (!data.ok) {
    setStatus(`Export failed: ${data.error}`);
    return;
  }
  setStatus(`Exported ${data.path} and ${data.bin_path}`);
}

function normalizeFlashOutput(text) {
  const lines = [];
  let current = "";
  for (let i = 0; i < text.length; i += 1) {
    const ch = text[i];
    if (ch === "\r") {
      current = "";
    } else if (ch === "\n") {
      if (current) lines.push(current);
      current = "";
    } else {
      current += ch;
    }
  }
  if (current) lines.push(current);
  return lines.join("\n");
}

function extractFlashPercent(text) {
  const matches = [...text.matchAll(/\]\s+(\d+(?:\.\d+)?)%/g)];
  if (matches.length === 0) return null;
  return parseFloat(matches[matches.length - 1][1]);
}

function setFlashProgress(percent) {
  const clamped = Math.max(0, Math.min(100, percent));
  const fill = document.getElementById("flash-progress-fill");
  const label = document.getElementById("flash-progress-label");
  if (fill) fill.style.width = `${clamped}%`;
  if (label) label.textContent = `${clamped.toFixed(1)}%`;
}

function openFlashModal(port) {
  const modal = document.getElementById("flash-modal");
  const logEl = document.getElementById("flash-log");
  const closeBtn = document.getElementById("btn-flash-close");
  const title = document.getElementById("flash-modal-title");
  flashOutputBuffer = "";
  if (title) title.textContent = `Flashing config to ${port}`;
  if (logEl) logEl.textContent = "";
  setFlashProgress(0);
  if (closeBtn) closeBtn.disabled = true;
  if (modal) modal.classList.add("open");
}

function closeFlashModal() {
  const modal = document.getElementById("flash-modal");
  if (modal) modal.classList.remove("open");
}

function appendFlashOutput(rawText) {
  const logEl = document.getElementById("flash-log");
  if (!logEl) return;

  flashOutputBuffer += rawText;

  const percent = extractFlashPercent(flashOutputBuffer);
  if (percent !== null) setFlashProgress(percent);

  logEl.textContent = normalizeFlashOutput(flashOutputBuffer);
  logEl.scrollTop = logEl.scrollHeight;
}

function flashConfigViaUsb(port) {
  flashOutputBuffer = "";
  const proto = location.protocol === "https:" ? "wss:" : "ws:";
  return new Promise((resolve, reject) => {
    const flashWs = new WebSocket(`${proto}//${location.host}/ws/flash`);
    let settled = false;

    const finish = (fn, value) => {
      if (settled) return;
      settled = true;
      fn(value);
    };

    flashWs.onopen = () => {
      appendFlashOutput(`[Flash] Starting programmer on ${port}…\n`);
      flashWs.send(JSON.stringify({ port }));
    };

    flashWs.onmessage = (ev) => {
      const msg = JSON.parse(ev.data);
      if (msg.type === "output" && msg.text) {
        appendFlashOutput(msg.text);
        return;
      }
      if (msg.type === "done") {
        appendFlashOutput("\n[Flash] Complete\n");
        setFlashProgress(100);
        flashWs.close();
        finish(resolve, msg);
        return;
      }
      if (msg.type === "error") {
        appendFlashOutput(`\n[Flash] ${msg.message || "Flash failed"}\n`);
        flashWs.close();
        finish(reject, new Error(msg.message || "Flash failed"));
      }
    };

    flashWs.onerror = () => {
      flashWs.close();
      finish(reject, new Error("Flash WebSocket connection failed"));
    };

    flashWs.onclose = () => {
      if (!settled) {
        finish(reject, new Error("Flash connection closed before completion"));
      }
    };
  });
}

async function flashConfig() {
  const port = document.getElementById("live-port").value;
  if (!port) {
    setStatus("Select a USB port before flashing");
    return;
  }

  const btnFlash = document.getElementById("btn-flash");
  const closeBtn = document.getElementById("btn-flash-close");
  btnFlash.disabled = true;
  openFlashModal(port);
  setStatus("Flashing…");

  try {
    const data = await flashConfigViaUsb(port);
    setStatus(`Flashed ${data.bin_path} to ${data.port}`);
  } catch (e) {
    setStatus(`Flash failed: ${e.message}`);
  } finally {
    btnFlash.disabled = false;
    if (closeBtn) closeBtn.disabled = false;
  }
}

async function loadDictionaries() {
  const [rec, bl, limits] = await Promise.all([
    fetch("/api/schema/recorder-dictionary").then((r) => r.json()),
    fetch("/api/schema/commands-dictionary").then((r) => r.json()),
    fetch("/api/limits").then((r) => r.json()).catch(() => null),
  ]);

  if (limits) {
    appState.limits = {
      max_steps: limits.max_steps ?? appState.limits.max_steps,
      max_command_bindings: limits.max_command_bindings ?? appState.limits.max_command_bindings,
      max_telemetry_bindings: limits.max_telemetry_bindings ?? appState.limits.max_telemetry_bindings,
      max_telemetry_fields: limits.max_telemetry_fields ?? appState.limits.max_telemetry_fields,
      max_var_slots: limits.max_var_slots ?? appState.limits.max_var_slots,
      comm_data_length: limits.comm_data_length ?? appState.limits.comm_data_length,
    };
  }

  appState.microCommands = (rec.recorder_commands || []).filter((c) => c.group === "micro commands");
  for (const cmd of rec.recorder_commands || []) {
    appState.commandMeta[cmd.name] = cmd;
  }
  appState.componentIds = rec.component_ids || [];
  renderConfigControls();

  appState.bluelinkCommands = bl.commands || [];
  appState.telemetryStructs = bl.telemetries || [];

  renderLiveExpressionGrid();
}

function renderLiveFields() {
  const select = document.getElementById("live-micro");
  const fieldsDiv = document.getElementById("live-fields");
  select.innerHTML = "";
  for (const op of appState.usbMicroOps) {
    const opt = document.createElement("option");
    opt.value = op.union_member;
    const cmd = Object.entries(UNION_BY_COMMAND).find(([, u]) => u === op.union_member);
    opt.textContent = cmd ? friendlyName(cmd[0]) : op.union_member;
    select.appendChild(opt);
  }
  select.addEventListener("change", () => updateLiveFields());
  updateLiveFields();
}

function parseByteArrayInput(raw) {
  const s = String(raw ?? "").trim();
  if (!s) return { bytes: [], fromQuotedString: false };
  if (s.startsWith('"') && s.endsWith('"')) {
    try {
      const text = JSON.parse(s);
      if (typeof text === "string") {
        return {
          bytes: [...text].map((c) => c.charCodeAt(0) & 0xff),
          fromQuotedString: true,
        };
      }
    } catch (_e) {
      /* fall through to comma-separated parsing */
    }
  }
  if (s.startsWith("'") && s.endsWith("'") && s.length >= 2) {
    const text = s.slice(1, -1);
    return {
      bytes: [...text].map((c) => c.charCodeAt(0) & 0xff),
      fromQuotedString: true,
    };
  }
  return {
    bytes: s
      .split(",")
      .map((part) => part.trim())
      .filter(Boolean),
    fromQuotedString: false,
  };
}

function collectLiveMicroFieldValues(op) {
  const values = {};
  let uartDataFromQuotedString = false;
  for (const f of op.fields || []) {
    const input = document.getElementById(`live-field-${f.name}`);
    if (!input) continue;
    if (input.type === "checkbox") {
      values[f.name] = input.checked ? 1 : 0;
      continue;
    }
    const raw = input.value.trim();
    const usesFormat = liveFieldUsesValueFormat(op, f);
    const formatSel = document.getElementById(`live-field-format-${f.name}`);
    const format = formatSel ? normalizeValueFormat(formatSel.value) : "int";

    if (f.array_size) {
      try {
        const maxLen = f.max_len || parseInt(String(f.array_size), 10) || 8;
        values[f.name] = parseBytesArrayWithFormat(raw, format, maxLen);
        if (
          op.union_member === "uart_transmit" &&
          f.name === "data" &&
          (format === "str" || parseByteArrayInput(raw).fromQuotedString)
        ) {
          uartDataFromQuotedString = true;
        }
      } catch (err) {
        throw new Error(`${f.name}: ${err.message || err}`);
      }
      continue;
    }

    if (usesFormat && op.union_member === "var_set" && f.name === "value") {
      try {
        values[f.name] = parseInt64WithFormat(raw, format, { allowByteList: formatAllowsByteList(format) });
      } catch (err) {
        throw new Error(`${f.name}: ${err.message || err}`);
      }
      continue;
    }

    if (liveFieldUsesHexAddress(op, f.name)) {
      try {
        values[f.name] = parseHexAddressInput(raw);
      } catch (err) {
        throw new Error(`${f.name}: ${err.message || err}`);
      }
      continue;
    }

    if (/^0x[0-9a-fA-F]+$/.test(raw)) values[f.name] = raw;
    else if (/^-?\d+$/.test(raw)) values[f.name] = parseInt(raw, 10);
    else values[f.name] = raw;
  }
  if (uartDataFromQuotedString && Array.isArray(values.data)) {
    values.length = values.data.length;
    const lengthInput = document.getElementById("live-field-length");
    if (lengthInput) lengthInput.value = String(values.length);
  }
  return values;
}

function updateLiveFields() {
  const unionMember = document.getElementById("live-micro").value;
  const op = appState.usbMicroOps.find((o) => o.union_member === unionMember);
  const fieldsDiv = document.getElementById("live-fields");
  fieldsDiv.innerHTML = "";
  if (!op || !op.fields) return;
  const hasUseVar = commandHasUseVarParam(op.fields);
  for (const f of op.fields) {
    const wrap = document.createElement("div");
    wrap.className = "field";
    const label = document.createElement("label");
    label.htmlFor = `live-field-${f.name}`;
    const maxLen = f.max_len || (f.array_size ? parseInt(String(f.array_size), 10) : null);
    const maxValue = f.max_value;
    const usesFormat = liveFieldUsesValueFormat(op, f);
    const usesHexAddress = liveFieldUsesHexAddress(op, f.name);
    const format = getLiveFieldFormat(op, f.name);

    if (f.name === "use_var") {
      wrap.classList.add("field-checkbox");
      label.className = "checkbox-label";
      const input = document.createElement("input");
      input.type = "checkbox";
      input.id = `live-field-${f.name}`;
      input.dataset.field = f.name;
      input.checked = isTruthyUseVar(f.default);
      label.appendChild(input);
      label.appendChild(document.createTextNode(" use var"));
      wrap.appendChild(label);
      input.addEventListener("change", () => {
        syncUseVarVarIndexVisibility(fieldsDiv, input.checked);
      });
      fieldsDiv.appendChild(wrap);
      continue;
    }

    if (f.name === "var_index" && hasUseVar) {
      wrap.dataset.useVarReveal = "var_index";
      wrap.hidden = !isTruthyUseVar(
        (() => {
          const cb = fieldsDiv.querySelector("#live-field-use_var");
          return cb ? cb.checked : f.default;
        })()
      );
      const hint = f.hint || (maxValue != null ? `max ${maxValue}` : null);
      if (hint) {
        label.innerHTML = `${f.name}<span class="limit-hint">${hint}</span>`;
      } else {
        label.textContent = f.name;
      }
      const input = document.createElement("input");
      input.type = "number";
      input.id = `live-field-${f.name}`;
      input.dataset.field = f.name;
      input.min = "0";
      if (maxValue != null) input.max = String(maxValue);
      input.value = f.default !== undefined && f.default !== null ? String(f.default) : "0";
      wrap.appendChild(label);
      wrap.appendChild(input);
      fieldsDiv.appendChild(wrap);
      continue;
    }

    const hint = f.hint || (usesHexAddress ? "hex (0x)" : maxLen ? `max ${maxLen} bytes` : maxValue != null ? `max ${maxValue}` : null);
    if (hint) {
      label.innerHTML = `${f.name}<span class="limit-hint">${hint}</span>`;
    } else {
      label.textContent = f.name;
    }

    let input;

    if (usesHexAddress) {
      input = document.createElement("input");
      input.id = `live-field-${f.name}`;
      input.dataset.field = f.name;
      input.dataset.hexAddress = "1";
      input.value = formatHexAddressValue(f.default ?? 0);
      input.placeholder = "0x12";
      input.addEventListener("change", () => {
        try {
          input.value = formatHexAddressValue(parseHexAddressInput(input.value));
        } catch (_) {
          /* keep value until send validates */
        }
      });
      wrap.appendChild(label);
      wrap.appendChild(input);
    } else if (usesFormat) {
      const row = document.createElement("div");
      row.className = "field-format-row";
      const formatSel = makeValueFormatSelect(`live-field-format-${f.name}`, format, `Format for ${f.name}`);
      formatSel.dataset.formatFor = f.name;
      input = document.createElement("input");
      input.id = `live-field-${f.name}`;
      input.dataset.field = f.name;
      input.className = "wide";
      if (f.array_size) {
        input.dataset.maxLen = String(maxLen || f.array_size);
        input.placeholder = `bytes (dec/hex) or "text"`;
        if (Array.isArray(f.default)) {
          input.value = formatBytesArrayForDisplay(f.default, format);
        }
      } else {
        input.placeholder = "int64 / double / text";
        input.value = formatInt64ForDisplay(f.default ?? 0, format);
      }
      formatSel.addEventListener("change", () => {
        const prevFormat = getLiveFieldFormat(op, f.name);
        const nextFormat = normalizeValueFormat(formatSel.value);
        setLiveFieldFormat(op, f.name, nextFormat);
        if (f.array_size) {
          try {
            const bytes = parseBytesArrayWithFormat(input.value, prevFormat, maxLen || 8);
            input.value = formatBytesArrayForDisplay(bytes, nextFormat);
          } catch (_) {
            input.value = "";
          }
        } else {
          try {
            const n = parseInt64WithFormat(input.value, prevFormat, {
              allowByteList: formatAllowsByteList(prevFormat),
            });
            input.value = formatInt64ForDisplay(n, nextFormat);
          } catch (_) {
            input.value = "";
          }
        }
      });
      row.appendChild(formatSel);
      row.appendChild(input);
      wrap.appendChild(label);
      wrap.appendChild(row);
    } else {
      input = document.createElement("input");
      input.id = `live-field-${f.name}`;
      input.dataset.field = f.name;
      input.value = f.default !== undefined && f.default !== null ? String(f.default) : "0";
      if (maxValue != null) {
        input.type = "number";
        input.min = "0";
        input.max = String(maxValue);
      }
      wrap.appendChild(label);
      wrap.appendChild(input);
    }
    fieldsDiv.appendChild(wrap);
  }
  const useVarCb = fieldsDiv.querySelector("#live-field-use_var");
  if (useVarCb) syncUseVarVarIndexVisibility(fieldsDiv, useVarCb.checked);
}

async function refreshUsbPorts() {
  const res = await fetch("/api/usb/ports");
  const data = await res.json();
  const select = document.getElementById("live-port");
  const prev = select.value;
  select.innerHTML = "";
  const ports = data.ports || [];
  if (!ports.length) {
    const opt = document.createElement("option");
    opt.value = "";
    opt.textContent = "(no ports found)";
    select.appendChild(opt);
    return;
  }
  for (const p of ports) {
    const device = typeof p === "string" ? p : p.device || p.port || "";
    const opt = document.createElement("option");
    opt.value = device;
    opt.textContent =
      typeof p === "object" && p.description ? `${device} — ${p.description}` : device;
    select.appendChild(opt);
  }
  if (prev && [...select.options].some((o) => o.value === prev)) select.value = prev;
}

async function refreshUsbStatus() {
  const res = await fetch("/api/usb/status");
  const data = await res.json();
  const opened = !!data.opened;
  const wasOpen = appState.usbOpen;
  appState.usbOpen = opened;
  document.getElementById("live-status").textContent = opened ? `Open: ${data.port}` : "Closed";
  document.getElementById("btn-live-open").disabled = opened;
  document.getElementById("btn-live-close").disabled = !opened;
  document.getElementById("btn-live-send").disabled = !opened;
  document.getElementById("btn-live-send-controller").disabled = !opened;
  if (!opened && wasOpen) {
    stopLiveExpressionStream();
    clearLiveExpressionValues();
  } else {
    updateLiveExpressionStatus();
  }
}

async function usbOpen() {
  const port = document.getElementById("live-port").value;
  if (!port) return;
  const res = await fetch("/api/usb/open", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ port }),
  });
  const data = await res.json();
  if (!data.ok) {
    setStatus(`USB open failed: ${data.error}`);
    await refreshUsbStatus();
    return;
  }
  await refreshUsbStatus();
  startLiveExpressionStream();
  setStatus(`USB open on ${port}; Live Expression streaming`);
}

async function usbClose() {
  stopLiveExpressionStream();
  await fetch("/api/usb/close", { method: "POST" });
  await refreshUsbStatus();
  clearLiveExpressionValues();
}

async function usbSendMicro() {
  const unionMember = document.getElementById("live-micro").value;
  const op = appState.usbMicroOps.find((o) => o.union_member === unionMember);
  if (!op) {
    setStatus("Select a micro command first");
    return;
  }
  let values;
  try {
    values = collectLiveMicroFieldValues(op);
  } catch (err) {
    setStatus(err.message || String(err));
    return;
  }
  setStatus("Sending micro command…");
  const res = await fetch("/api/usb/send-micro", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      union_member: op.union_member,
      values,
      destination_component: "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER",
      qos: "none",
    }),
  });
  const data = await res.json();
  if (data.ok) {
    setStatus(`Sent ${data.payload_type}${data.payload_hex ? ` (${data.payload_hex})` : ""}`);
  } else {
    setStatus(`Send failed: ${data.error}`);
  }
}

function renderLiveControllerFields() {
  const select = document.getElementById("live-controller");
  select.innerHTML = "";
  for (const cmd of appState.usbControllerCmds) {
    const opt = document.createElement("option");
    opt.value = cmd.payload_type;
    opt.textContent = cmd.label || cmd.payload_type;
    select.appendChild(opt);
  }
  select.addEventListener("change", () => updateLiveControllerFields());
  updateLiveControllerFields();
}

function updateLiveControllerFields() {
  const payloadType = document.getElementById("live-controller").value;
  const cmd = appState.usbControllerCmds.find((c) => c.payload_type === payloadType);
  const fieldsDiv = document.getElementById("live-controller-fields");
  fieldsDiv.innerHTML = "";
  if (!cmd || !cmd.fields) return;
  for (const f of cmd.fields) {
    const wrap = document.createElement("div");
    wrap.className = "field";
    const label = document.createElement("label");
    label.textContent = f.name;
    let input;
    if (f.enum_values && f.enum_values.length) {
      input = document.createElement("select");
      for (const ev of f.enum_values) {
        const opt = document.createElement("option");
        opt.value = ev;
        opt.textContent = ev;
        input.appendChild(opt);
      }
      if (f.default !== undefined && f.default !== null) input.value = String(f.default);
    } else {
      input = document.createElement("input");
      if (f.array_size) {
        input.className = "wide";
        input.placeholder = `comma-separated (${f.array_size})`;
        if (Array.isArray(f.default)) input.value = f.default.join(",");
      } else {
        input.value = f.default !== undefined && f.default !== null ? String(f.default) : "0";
      }
    }
    input.dataset.field = f.name;
    wrap.appendChild(label);
    wrap.appendChild(input);
    fieldsDiv.appendChild(wrap);
  }
}

async function usbSendController() {
  const payloadType = document.getElementById("live-controller").value;
  const cmd = appState.usbControllerCmds.find((c) => c.payload_type === payloadType);
  if (!cmd) return;
  const values = {};
  document.querySelectorAll("#live-controller-fields [data-field]").forEach((el) => {
    const field = (cmd.fields || []).find((f) => f.name === el.dataset.field);
    let v = el.value.trim();
    if (field && field.array_size) {
      values[el.dataset.field] = v ? v.split(",").map((s) => s.trim()) : [];
      return;
    }
    if (/^0x[0-9a-fA-F]+$/.test(v)) values[el.dataset.field] = v;
    else if (/^-?\d+$/.test(v)) values[el.dataset.field] = parseInt(v, 10);
    else values[el.dataset.field] = v;
  });
  setStatus("Sending controller command…");
  const res = await fetch("/api/usb/send-controller", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ payload_type: payloadType, values, qos: "none" }),
  });
  const data = await res.json();
  if (data.ok) {
    setStatus(`Sent ${data.payload_type}${data.payload_hex ? ` (${data.payload_hex})` : ""}`);
  } else {
    setStatus(`Send failed: ${data.error}`);
  }
}

function wireEvents() {
  document.getElementById("config-name")?.addEventListener("change", syncConfigFromControls);
  document.getElementById("config-component")?.addEventListener("change", syncConfigFromControls);
  document.getElementById("btn-load").addEventListener("click", () => loadGraph());
  document.getElementById("btn-examples").addEventListener("click", (e) => {
    e.stopPropagation();
    toggleExamplesMenu();
  });
  document.getElementById("btn-save-example").addEventListener("click", (e) => {
    e.stopPropagation();
    toggleSaveExampleMenu();
  });
  document.getElementById("btn-confirm-save-example").addEventListener("click", confirmSaveToExample);
  document.getElementById("btn-cancel-save-example").addEventListener("click", closeSaveExampleConfirm);
  document.getElementById("confirm-save-example-modal")?.addEventListener("click", (e) => {
    if (e.target === e.currentTarget) closeSaveExampleConfirm();
  });
  document.addEventListener("click", (e) => {
    const wraps = document.querySelectorAll(".examples-wrap");
    const inside = Array.from(wraps).some((wrap) => wrap.contains(e.target));
    if (!inside) closeAllExampleMenus();
  });
  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") {
      const confirmOpen = document.getElementById("confirm-save-example-modal")?.classList.contains("open");
      if (confirmOpen) {
        closeSaveExampleConfirm();
        return;
      }
      closeAllExampleMenus();
      return;
    }

    const editorOpen = document.getElementById("editor-modal")?.classList.contains("open");
    if (!editorOpen) return;
    if (!(e.ctrlKey || e.metaKey) || e.altKey) return;

    const tag = (document.activeElement?.tagName || "").toLowerCase();
    if (tag === "input" || tag === "textarea" || tag === "select") return;

    const key = e.key.toLowerCase();
    if (key === "c") {
      e.preventDefault();
      copySelectedCommand();
      return;
    }
    if (key === "v") {
      e.preventDefault();
      pasteCommands();
      return;
    }
    if (key === "z") {
      e.preventDefault();
      undoEditor();
    }
  });
  document.getElementById("btn-export").addEventListener("click", exportGraph);
  document.getElementById("btn-flash").addEventListener("click", flashConfig);
  document.getElementById("btn-flash-close").addEventListener("click", closeFlashModal);
  document.getElementById("btn-modal-save").addEventListener("click", () => closeEditor(true));
  document.getElementById("btn-modal-close").addEventListener("click", () => closeEditor(false));
  document.getElementById("btn-modal-edit-binding").addEventListener("click", () => {
    const id = appState.activeContainerId;
    if (!id) return;
    openCommandBindingEditor(id);
  });
  document.getElementById("btn-modal-remove").addEventListener("click", () => {
    const id = appState.activeContainerId;
    if (!id) return;
    if (removeBinding(id)) closeEditor(false);
  });
  document.getElementById("btn-bind-save").addEventListener("click", () => closeBindModal(true));
  document.getElementById("btn-bind-close").addEventListener("click", () => closeBindModal(false));
  document.getElementById("btn-bind-remove").addEventListener("click", () => {
    const id = appState.activeContainerId;
    if (!id) return;
    const editorWasOpen = document.getElementById("editor-modal")?.classList.contains("open");
    if (removeBinding(id)) {
      closeBindModal(false);
      if (editorWasOpen) closeEditor(false);
    }
  });
  document.getElementById("btn-live-refresh").addEventListener("click", refreshUsbPorts);
  document.getElementById("btn-live-open").addEventListener("click", usbOpen);
  document.getElementById("btn-live-close").addEventListener("click", usbClose);
  document.getElementById("btn-live-send").addEventListener("click", usbSendMicro);
  document.getElementById("btn-live-send-controller").addEventListener("click", usbSendController);
}

async function init() {
  wireEvents();
  ensureFixedContainers();
  await loadDictionaries();
  renderBoard();

  const microRes = await fetch("/api/usb/micro-ops");
  const microData = await microRes.json();
  appState.usbMicroOps = microData.micro_ops || [];
  renderLiveFields();

  const ctrlRes = await fetch("/api/usb/controller-commands");
  const ctrlData = await ctrlRes.json();
  appState.usbControllerCmds = ctrlData.controller_commands || [];
  renderLiveControllerFields();

  await refreshUsbPorts();
  await refreshUsbStatus();
  await loadGraph();
}

init().catch((err) => setStatus(`Init error: ${err.message}`));

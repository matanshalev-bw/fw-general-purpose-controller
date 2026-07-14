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
  var_set: "var set",
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
  activeContainerId: null,
  bindMode: null,
  commandCounter: 0,
  telemetryCounter: 0,
};

let drawflowEditor = null;
let selectedNodeId = null;

function setStatus(msg) {
  document.getElementById("status").textContent = msg;
}

function friendlyName(command) {
  return FRIENDLY_NAMES[command] || command.replace(/_/g, " ");
}

function defaultArgs(meta) {
  const args = {};
  if (!meta || !meta.params) return args;
  for (const p of meta.params) {
    if (p.has_default) args[p.name] = p.default;
    else if (p.annotation === "int" || p.annotation === "float") args[p.name] = 0;
    else if (p.is_list) args[p.name] = [];
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
    return container.trigger ? `${container.trigger} @ ${container.rate || "?"} Hz` : "not configured";
  }
  if (container.type === "command") {
    const trig = container.trigger || "no trigger";
    const n = (container.steps || []).length;
    return `${trig} · ${n} step${n === 1 ? "" : "s"}`;
  }
  const steps = container.steps || [];
  if (!steps.length) return "empty — click to edit";
  return steps.map(stepSummary).join(" → ");
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

  for (const slot of FIXED_SLOTS) {
    const c = appState.containers[slot.id];
    const card = document.createElement("div");
    card.className = `state-card ${slot.cardClass || ""}`;
    card.style.gridArea = slot.area;
    if ((c.steps || []).length > 0) card.classList.add("has-steps");
    card.innerHTML = `
      <div class="card-title">${c.label}</div>
      <div class="card-meta">${(c.steps || []).length} step(s)</div>
      <div class="card-preview">${containerPreview(c)}</div>
    `;
    card.appendChild(makeStateArrow(slot.repeatable, slot.arrowUp));
    card.addEventListener("click", () => openEditor(c.id));
    board.appendChild(card);
  }

  const commandZone = document.createElement("div");
  commandZone.className = "board-zone zone-command";
  const commandContainers = Object.values(appState.containers).filter((c) => c.type === "command");
  for (const c of commandContainers) {
    const card = document.createElement("div");
    card.className = "state-card type-command";
    if ((c.steps || []).length) card.classList.add("has-steps");
    card.innerHTML = `
      <div class="card-title">${c.label || "COMMAND"}</div>
      <div class="card-meta">${(c.steps || []).length} step(s)</div>
      <div class="card-preview">${containerPreview(c)}</div>
    `;
    card.appendChild(makeStateArrow(false));
    card.addEventListener("click", () => openEditor(c.id));
    commandZone.appendChild(card);
  }
  const addCmd = document.createElement("div");
  addCmd.className = "add-card";
  addCmd.textContent = "+ Add COMMAND binding";
  addCmd.addEventListener("click", () => openBindModal("command"));
  commandZone.appendChild(addCmd);
  board.appendChild(commandZone);

  const telemetryZone = document.createElement("div");
  telemetryZone.className = "board-zone zone-telemetry";
  const telemetryContainers = Object.values(appState.containers).filter((c) => c.type === "telemetry");
  for (const c of telemetryContainers) {
    const card = document.createElement("div");
    card.className = "state-card type-telemetry";
    if (c.trigger) card.classList.add("has-steps");
    card.innerHTML = `
      <div class="card-title">${c.label || "TELEMETRY"}</div>
      <div class="card-meta">telemetry binding</div>
      <div class="card-preview">${containerPreview(c)}</div>
    `;
    card.appendChild(makeStateArrow(true));
    card.addEventListener("click", () => openTelemetryEditor(c.id));
    telemetryZone.appendChild(card);
  }
  const addTel = document.createElement("div");
  addTel.className = "add-card";
  addTel.textContent = "+ Add TELEMETRY binding";
  addTel.addEventListener("click", () => openBindModal("telemetry"));
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

function renderPalette() {
  const bank = document.getElementById("palette-bank");
  bank.innerHTML = "";
  for (const cmd of appState.microCommands) {
    if (PALETTE_SKIP.has(cmd.name)) continue;
    makePaletteItem(cmd.name, cmd, bank);
  }
}

function renderModalPalette() {
  const panel = document.getElementById("modal-palette");
  panel.innerHTML = "";
  for (const cmd of appState.microCommands) {
    if (PALETTE_SKIP.has(cmd.name)) continue;
    makePaletteItem(cmd.name, cmd, panel);
  }
}

function formatNodeLabel(step) {
  const name = friendlyName(step.command);
  const args = step.args || {};
  if (step.command === "if_condition") {
    const op = args.comparing_type || ">=";
    return `IF var${args.first_var_index ?? 0} ${op} var${args.second_var_index ?? 0}`;
  }
  if (step.command === "end_condition") return "end IF";
  const keys = Object.keys(args).filter((k) => k !== "reserved");
  if (!keys.length) return name;
  return `${name}\n${keys
    .slice(0, 2)
    .map((k) => `${k}=${args[k]}`)
    .join(", ")}`;
}

function nodeClassForCommand(command) {
  if (command === "if_condition") return "node-if";
  if (command === "end_condition") return "node-end";
  return "node-step";
}

function buildDrawflowFromSteps(steps) {
  if (!drawflowEditor) return;
  drawflowEditor.clear();
  selectedNodeId = null;
  renderPropsPanel(null);

  let prevId = null;
  let x = 60;
  let y = 80;
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
}

function linearizeDrawflow() {
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

  const steps = [];
  let currentId = startId;
  const visited = new Set();
  while (currentId && !visited.has(currentId)) {
    visited.add(currentId);
    const node = data[currentId];
    const command = node.data?.command;
    if (command) {
      steps.push({
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
  return steps;
}

function addNodeAtDrop(command, args, clientX, clientY) {
  if (!drawflowEditor) return;
  const precanvas = drawflowEditor.precanvas;
  const rect = precanvas.getBoundingClientRect();
  const zoom = drawflowEditor.zoom || 1;
  const x = (clientX - rect.left) / zoom;
  const y = (clientY - rect.top) / zoom;
  const meta = appState.commandMeta[command];
  const step = { command, args: args || defaultArgs(meta) };
  const html = `<div class="node-label">${formatNodeLabel(step).replace(/\n/g, "<br>")}</div>`;
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
}

function renderPropsPanel(nodeId) {
  const panel = document.getElementById("props-panel");
  if (!nodeId || !drawflowEditor) {
    panel.innerHTML = `<h3>Properties</h3><p class="props-empty">Select a node to edit parameters.</p>`;
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

  const fields = meta.params
    .map((p) => {
      const val = node.data.args[p.name];
      const displayVal = Array.isArray(val) ? val.join(", ") : val ?? "";
      if (p.is_list) {
        return `
          <div class="field">
            <label for="prop-${p.name}">${p.name}</label>
            <input id="prop-${p.name}" data-param="${p.name}" data-is-list="1" class="wide"
              value="${displayVal}" placeholder="comma-separated bytes" />
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
      return `
        <div class="field">
          <label for="prop-${p.name}">${p.name}</label>
          <input id="prop-${p.name}" data-param="${p.name}" value="${displayVal}" />
        </div>`;
    })
    .join("");

  panel.innerHTML = `<h3>${friendlyName(command)}</h3>${fields}`;

  panel.querySelectorAll("[data-param]").forEach((el) => {
    const handler = () => {
      const param = el.dataset.param;
      let value;
      if (el.dataset.isList === "1") {
        value = el.value
          .split(",")
          .map((s) => s.trim())
          .filter((s) => s.length)
          .map((s) => (s.startsWith("0x") ? parseInt(s, 16) : parseInt(s, 10) || 0));
      } else if (el.tagName === "SELECT") {
        value = el.value;
      } else if (el.value.startsWith("0x")) {
        value = parseInt(el.value, 16);
      } else if (el.value.includes(".")) {
        value = parseFloat(el.value);
      } else {
        value = parseInt(el.value, 10);
        if (Number.isNaN(value)) value = el.value;
      }
      // getNodeFromId returns a deep clone — write back via updateNodeDataFromId
      // so Save / linearizeDrawflow see the edited args instead of defaults.
      node.data.args[param] = value;
      drawflowEditor.updateNodeDataFromId(nodeId, node.data);
      const label = formatNodeLabel({ command, args: node.data.args });
      const nodeEl = document.getElementById(`node-${nodeId}`);
      const inner = nodeEl?.querySelector(".node-label");
      if (inner) inner.innerHTML = label.replace(/\n/g, "<br>");
    };
    el.addEventListener("input", handler);
    el.addEventListener("change", handler);
  });
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
  container.addEventListener("drop", (ev) => {
    ev.preventDefault();
    const raw = ev.dataTransfer.getData("application/gpc-command");
    if (!raw) return;
    const payload = JSON.parse(raw);
    addNodeAtDrop(payload.command, payload.args, ev.clientX, ev.clientY);
  });

  drawflowEditor.on("nodeSelected", (id) => {
    selectedNodeId = id;
    renderPropsPanel(id);
  });
  drawflowEditor.on("nodeUnselected", () => {
    selectedNodeId = null;
    renderPropsPanel(null);
  });
}

function openEditor(containerId) {
  const container = appState.containers[containerId];
  if (!container) return;
  if (container.type === "telemetry") {
    openTelemetryEditor(containerId);
    return;
  }

  appState.activeContainerId = containerId;
  document.getElementById("modal-title").textContent = `${container.label} — sequence editor`;
  document.getElementById("editor-modal").classList.add("open");
  renderModalPalette();

  if (!drawflowEditor) initDrawflow();
  else drawflowEditor.clear();

  buildDrawflowFromSteps(container.steps || []);
}

function closeEditor(save) {
  if (save && appState.activeContainerId) {
    const steps = linearizeDrawflow();
    const container = appState.containers[appState.activeContainerId];
    if (container) container.steps = steps;
    renderBoard();
    setStatus(`Saved ${container.label} (${steps.length} steps)`);
  }
  appState.activeContainerId = null;
  document.getElementById("editor-modal").classList.remove("open");
}

function openTelemetryEditor(containerId) {
  const container = appState.containers[containerId];
  if (!container) return;
  appState.bindMode = "telemetry-edit";
  appState.activeContainerId = containerId;
  document.getElementById("bind-modal-title").textContent = `Edit telemetry: ${container.label}`;
  renderBindForm(container);
  document.getElementById("bind-modal").classList.add("open");
}

function openBindModal(mode) {
  appState.bindMode = mode;
  appState.activeContainerId = null;
  document.getElementById("bind-modal-title").textContent =
    mode === "command" ? "Add COMMAND binding" : "Add TELEMETRY binding";
  renderBindForm(null);
  document.getElementById("bind-modal").classList.add("open");
}

function renderBindForm(existing) {
  const form = document.getElementById("bind-form");
  const isCommand = appState.bindMode === "command" || (existing && existing.type === "command");
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
      const key = isTelemetry ? `${f.name}_var_index` : f.name;
      const val = existing?.fields?.[key] ?? (isTelemetry ? 0 : f.default ?? 0);
      if (isTelemetry) {
        extraFields += `
          <div class="field">
            <label for="bind-${key}">${f.name} var_index</label>
            <input id="bind-${key}" data-field="${key}" type="number" min="0" max="7" value="${val}" />
          </div>`;
      } else {
        extraFields += `
          <div class="field">
            <label for="bind-${f.name}">${f.name}</label>
            <input id="bind-${f.name}" data-field="${f.name}" value="${val}" />
          </div>`;
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
    const trig = document.getElementById("bind-trigger").value;
    const partial = existing ? { ...existing, trigger: trig, fields: {} } : null;
    if (partial) partial.trigger = trig;
    else {
      renderBindForm(isCommand ? { type: "command", trigger: trig, fields: {} } : { type: "telemetry", trigger: trig, rate: 1, fields: {} });
      return;
    }
    renderBindForm(partial);
  });
}

function closeBindModal(save) {
  if (save) {
    const trigger = document.getElementById("bind-trigger")?.value;
    const fields = {};
    document.querySelectorAll("#bind-form [data-field]").forEach((el) => {
      const key = el.dataset.field;
      let val = el.value;
      if (el.type === "number") val = parseInt(val, 10) || 0;
      else if (val === "true" || val === "false") val = val === "true";
      else if (/^\d+$/.test(val)) val = parseInt(val, 10);
      fields[key] = val;
    });

    if (appState.bindMode === "telemetry-edit" && appState.activeContainerId) {
      const c = appState.containers[appState.activeContainerId];
      c.trigger = trigger;
      c.rate = parseInt(document.getElementById("bind-rate")?.value || "1", 10);
      c.fields = fields;
      c.label = `TELEMETRY: ${trigger}`;
    } else if (appState.bindMode === "command") {
      const id = `command_${appState.commandCounter++}`;
      appState.containers[id] = {
        id,
        type: "command",
        label: `COMMAND: ${trigger}`,
        trigger,
        fields,
        steps: [],
      };
    } else if (appState.bindMode === "telemetry") {
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
    }
    renderBoard();
    setStatus("Binding added");
  }
  appState.bindMode = null;
  document.getElementById("bind-modal").classList.remove("open");
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

  return {
    config: appState.config,
    containers,
  };
}

function applyLoadedGraph(graph) {
  appState.config = graph.config || appState.config;
  appState.containers = {};
  appState.commandCounter = 0;
  appState.telemetryCounter = 0;
  ensureFixedContainers();

  for (const c of graph.containers || []) {
    if (c.type === "command") {
      const id = c.id || `command_${appState.commandCounter++}`;
      appState.containers[id] = { ...c, id, steps: c.steps || [] };
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
}

async function loadGraph() {
  setStatus("Loading config…");
  const res = await fetch("/api/graph/load");
  const data = await res.json();
  if (!data.ok) {
    setStatus(`Load failed: ${data.error}`);
    return;
  }
  applyLoadedGraph(data.graph);
  setStatus(`Loaded ${(data.graph.containers || []).length} container(s)`);
}

async function exportGraph() {
  const graph = buildGraphPayload();
  if (!graph.containers.length) {
    setStatus("Nothing to export — add at least one sequence or binding");
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

async function flashConfig() {
  const port = document.getElementById("live-port").value;
  if (!port) {
    setStatus("Select a USB port before flashing");
    return;
  }
  setStatus("Flashing…");
  const res = await fetch("/api/flash", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ port }),
  });
  const data = await res.json();
  if (!data.ok) {
    setStatus(`Flash failed: ${data.error}`);
    return;
  }
  setStatus(data.message || "Flash complete");
}

async function loadDictionaries() {
  const [rec, bl] = await Promise.all([
    fetch("/api/schema/recorder-dictionary").then((r) => r.json()),
    fetch("/api/schema/commands-dictionary").then((r) => r.json()),
  ]);

  appState.microCommands = (rec.recorder_commands || []).filter((c) => c.group === "micro commands");
  for (const cmd of rec.recorder_commands || []) {
    appState.commandMeta[cmd.name] = cmd;
  }

  appState.bluelinkCommands = bl.commands || [];
  appState.telemetryStructs = bl.telemetries || [];

  renderPalette();
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

function updateLiveFields() {
  const unionMember = document.getElementById("live-micro").value;
  const op = appState.usbMicroOps.find((o) => o.union_member === unionMember);
  const fieldsDiv = document.getElementById("live-fields");
  fieldsDiv.innerHTML = "";
  if (!op || !op.fields) return;
  for (const f of op.fields) {
    const wrap = document.createElement("div");
    wrap.className = "field";
    const label = document.createElement("label");
    label.textContent = f.name;
    const input = document.createElement("input");
    input.dataset.field = f.name;
    input.value = f.default ?? 0;
    if (f.array_size) input.className = "wide";
    wrap.appendChild(label);
    wrap.appendChild(input);
    fieldsDiv.appendChild(wrap);
  }
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
  appState.usbOpen = opened;
  document.getElementById("live-status").textContent = opened ? `Open: ${data.port}` : "Closed";
  document.getElementById("btn-live-open").disabled = opened;
  document.getElementById("btn-live-close").disabled = !opened;
  document.getElementById("btn-live-send").disabled = !opened;
  document.getElementById("btn-live-send-controller").disabled = !opened;
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
  if (!data.ok) setStatus(`USB open failed: ${data.error}`);
  await refreshUsbStatus();
}

async function usbClose() {
  await fetch("/api/usb/close", { method: "POST" });
  await refreshUsbStatus();
}

async function usbSendMicro() {
  const unionMember = document.getElementById("live-micro").value;
  const values = {};
  document.querySelectorAll("#live-fields [data-field]").forEach((el) => {
    let v = el.value;
    if (v.startsWith("0x")) v = parseInt(v, 16);
    else if (/^\d+$/.test(v)) v = parseInt(v, 10);
    values[el.dataset.field] = v;
  });
  const res = await fetch("/api/usb/send-micro", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ union_member: unionMember, values }),
  });
  const data = await res.json();
  setStatus(data.ok ? "Micro command sent" : `Send failed: ${data.error}`);
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
  document.getElementById("btn-load").addEventListener("click", loadGraph);
  document.getElementById("btn-export").addEventListener("click", exportGraph);
  document.getElementById("btn-flash").addEventListener("click", flashConfig);
  document.getElementById("btn-modal-save").addEventListener("click", () => closeEditor(true));
  document.getElementById("btn-modal-close").addEventListener("click", () => closeEditor(false));
  document.getElementById("btn-bind-save").addEventListener("click", () => closeBindModal(true));
  document.getElementById("btn-bind-close").addEventListener("click", () => closeBindModal(false));
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

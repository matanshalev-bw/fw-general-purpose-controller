(function () {
  const term = new Terminal({
    theme: { background: "#1e1e1e", foreground: "#d4d4d4", cursor: "#aeafad" },
    fontFamily: "Consolas, Monaco, monospace",
    fontSize: 13,
    cursorBlink: true,
  });
  const fitAddon = new FitAddon.FitAddon();
  term.loadAddon(fitAddon);
  term.open(document.getElementById("terminal"));
  fitAddon.fit();

  const previewEl = document.getElementById("preview");
  const statusEl = document.getElementById("status");
  const bindingsEl = document.getElementById("bindings-list");

  function updateBindings(powerup, bindings) {
    const parts = [];
    if (powerup) {
      const cls = powerup.in_progress ? ' class="recording"' : "";
      const tag = powerup.in_progress ? " *" : "";
      parts.push(`<span${cls}>Powerup: ${powerup.step_count} steps${tag}</span>`);
    }
    if (bindings && bindings.length > 0) {
      bindings.forEach((b) => {
        const cls = b.in_progress ? ' class="recording"' : "";
        const tag = b.in_progress ? " *" : "";
        parts.push(`<span${cls}>${b.index}: ${b.payload_type} (${b.step_count})${tag}</span>`);
      });
    }
    bindingsEl.innerHTML = parts.length ? parts.join(" ") : "No powerup or bindings";
  }
  let ws;
  let lineBuffer = "";
  const prompt = ">>> ";

  /** Identifier fragment being completed (suffix of lineBuffer). */
  function currentWord(buf) {
    const m = buf.match(/[A-Za-z_][A-Za-z0-9_]*$/);
    return m ? m[0] : "";
  }

  function applyCompletion(match) {
    const word = currentWord(lineBuffer);
    if (!match.startsWith(word)) {
      return;
    }
    const suffix = match.slice(word.length);
    lineBuffer = lineBuffer.slice(0, lineBuffer.length - word.length) + match;
    term.write(suffix);
  }

  function connect() {
    const proto = location.protocol === "https:" ? "wss:" : "ws:";
    ws = new WebSocket(`${proto}//${location.host}/ws/repl`);
    ws.onopen = () => {
      statusEl.textContent = "Connected";
      term.writeln("GPC Sequence Recorder — type help() for commands.");
      writePrompt();
    };
    ws.onclose = () => {
      statusEl.textContent = "Disconnected — refresh to reconnect";
    };
    ws.onmessage = (ev) => {
      const msg = JSON.parse(ev.data);
      if (msg.type === "complete") {
        if (msg.matches.length === 1) {
          applyCompletion(msg.matches[0]);
        } else if (msg.common_prefix && msg.common_prefix.length > currentWord(lineBuffer).length) {
          applyCompletion(msg.common_prefix);
        } else if (msg.matches.length > 1) {
          term.writeln("\r\n" + msg.matches.join("  "));
          term.write("\r\n" + prompt + lineBuffer);
        }
        return;
      }
      if (msg.type === "result") {
        if (msg.output) {
          msg.output.split("\n").forEach((l) => term.writeln(l));
        }
        if (msg.preview) previewEl.textContent = msg.preview;
        if (msg.powerup !== undefined || msg.bindings) {
          updateBindings(msg.powerup, msg.bindings);
        }
        if (msg.continue) writePrompt();
      }
    };
  }

  function writePrompt() {
    term.write("\r\n" + prompt);
    lineBuffer = "";
  }

  term.onData((data) => {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    if (data === "\r") {
      term.write("\r\n");
      const line = lineBuffer.trim();
      ws.send(line);
      lineBuffer = "";
      return;
    }
    if (data === "\u007f") {
      if (lineBuffer.length > 0) {
        lineBuffer = lineBuffer.slice(0, -1);
        term.write("\b \b");
      }
      return;
    }
    if (data === "\t") {
      ws.send("\t");
      ws.send(lineBuffer);
      return;
    }
    if (data >= " ") {
      lineBuffer += data;
      term.write(data);
    }
  });

  window.addEventListener("resize", () => fitAddon.fit());

  document.getElementById("btn-export").addEventListener("click", async () => {
    const path = "configs/ConfigsTypes/g474_gpc_config_memory.hpp";
    const hexPath = "config_projects/config_g474/Debug/config_g474.hex";
    if (
      !confirm(
        `Export to ${path} and ${hexPath}? This will overwrite both files.`
      )
    ) {
      return;
    }
    statusEl.textContent = "Exporting…";
    const res = await fetch("/api/export", { method: "POST" });
    const data = await res.json();
    if (data.ok) {
      statusEl.textContent = `Exported to ${data.path} and ${data.hex_path}. ${data.flash_note || ""}`;
      previewEl.textContent = data.hpp;
    } else {
      statusEl.textContent = `Export failed: ${data.error}`;
    }
  });

  document.getElementById("btn-help").addEventListener("click", () => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send("help()");
    }
  });

  // --- BlueLink struct dictionary ---
  const dictOverlay = document.getElementById("dict-overlay");
  const dictBody = document.getElementById("dict-body");
  const dictFilter = document.getElementById("dict-filter");
  let dictData = null;

  function formatFieldLine(field) {
    let line = `<span class="type">${field.type}</span> ${field.name}`;
    if (field.array_size) {
      line += `[${field.array_size}]`;
    }
    if (field.default !== null && field.default !== undefined && field.default !== "") {
      line += ` <span class="def">= ${field.default}</span>`;
    }
    let html = `<div class="field">${line}</div>`;
    if (field.enum_values && field.enum_values.length) {
      const typeName = field.type.split("::").pop();
      html += `<div class="enum-values"><div class="enum-label">enum ${typeName}</div>`;
      field.enum_values.forEach((ev) => {
        html += `<div class="enum-val">${ev.name} = ${ev.value}</div>`;
      });
      html += "</div>";
    }
    return html;
  }

  function fieldSearchText(field) {
    let text = `${field.type} ${field.name}`;
    if (field.enum_values) {
      field.enum_values.forEach((ev) => {
        text += ` ${ev.name} ${ev.value}`;
      });
    }
    return text;
  }

  function renderDictEntry(title, meta, fields) {
    const fieldsHtml =
      fields && fields.length
        ? fields.map(formatFieldLine).join("")
        : '<div class="field dict-empty">(no fields)</div>';
    return `<div class="dict-entry" data-search="${meta.toLowerCase()} ${title.toLowerCase()}">
      <div class="title">${title}</div>
      <div class="meta">${meta}</div>
      ${fieldsHtml}
    </div>`;
  }

  function renderDictionary(data, filter) {
    if (!dictBody) return;
    const q = (filter || "").trim().toLowerCase();
    const match = (text) => !q || text.includes(q);

    let html = "";

    const commands = data.commands || [];
    const cmdEntries = commands
      .map((c) => {
        const title = c.payload_type || c.struct_name;
        const idPart = c.payload_type_id != null ? ` · id ${c.payload_type_id}` : "";
        const meta = `struct ${c.struct_name}${idPart}`;
        const search = `${title} ${meta} ${(c.fields || []).map(fieldSearchText).join(" ")}`;
        return { title, meta, fields: c.fields, search: search.toLowerCase() };
      })
      .filter((e) => match(e.search));

    html += '<div class="dict-section"><h3>Commands (CommandsPayload)</h3>';
    if (!cmdEntries.length) {
      html += '<p class="dict-empty">No matching commands.</p>';
    } else {
      html += cmdEntries
        .map((e) => renderDictEntry(e.title, e.meta, e.fields))
        .join("");
    }
    html += "</div>";

    const micro = data.micro_ops || [];
    const microEntries = micro
      .map((m) => {
        const title = m.payload_type || m.union_member;
        const idPart = m.payload_type_id != null ? ` · id ${m.payload_type_id}` : "";
        const meta = `struct ${m.struct_name} · ${m.union_member}()${idPart}`;
        const search = `${title} ${meta} ${(m.fields || []).map(fieldSearchText).join(" ")}`;
        return { title, meta, fields: m.fields, search: search.toLowerCase() };
      })
      .filter((e) => match(e.search));

    html += '<div class="dict-section"><h3>Micro operations (MicroOpsPayload)</h3>';
    if (!microEntries.length) {
      html += '<p class="dict-empty">No matching micro ops.</p>';
    } else {
      html += microEntries
        .map((e) => renderDictEntry(e.title, e.meta, e.fields))
        .join("");
    }
    html += "</div>";

    dictBody.innerHTML = html;
  }

  async function loadDictionary() {
    if (dictData) {
      renderDictionary(dictData, dictFilter ? dictFilter.value : "");
      return;
    }
    if (dictBody) dictBody.innerHTML = '<p class="dict-empty">Loading…</p>';
    try {
      const res = await fetch("/api/schema/commands-dictionary");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      dictData = await res.json();
      renderDictionary(dictData, dictFilter ? dictFilter.value : "");
    } catch (e) {
      if (dictBody) {
        dictBody.innerHTML = `<p class="dict-empty">Failed to load dictionary: ${e.message}</p>`;
      }
    }
  }

  function openDictionary() {
    if (!dictOverlay) return;
    dictOverlay.classList.add("open");
    loadDictionary();
    if (dictFilter) {
      dictFilter.value = "";
      dictFilter.focus();
    }
  }

  function closeDictionary() {
    if (dictOverlay) dictOverlay.classList.remove("open");
  }

  document.getElementById("btn-dictionary")?.addEventListener("click", openDictionary);
  document.getElementById("btn-dict-close")?.addEventListener("click", closeDictionary);
  dictOverlay?.addEventListener("click", (e) => {
    if (e.target === dictOverlay) closeDictionary();
  });
  dictFilter?.addEventListener("input", () => {
    if (dictData) renderDictionary(dictData, dictFilter.value);
  });
  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape" && dictOverlay?.classList.contains("open")) {
      closeDictionary();
    }
  });

  // --- USB immediate micro commands ---
  const usbPortEl = document.getElementById("usb-port");
  const usbStatusEl = document.getElementById("usb-status");
  const usbMicroOpEl = document.getElementById("usb-micro-op");
  const usbMicroFieldsEl = document.getElementById("usb-micro-fields");
  const usbControllerCmdEl = document.getElementById("usb-controller-cmd");
  const usbControllerFieldsEl = document.getElementById("usb-controller-fields");
  const btnUsbOpen = document.getElementById("btn-usb-open");
  const btnUsbClose = document.getElementById("btn-usb-close");
  const btnUsbSend = document.getElementById("btn-usb-send");
  const btnUsbSendController = document.getElementById("btn-usb-send-controller");

  /** Built-in catalog so the controller dropdown works before/without API. */
  const FALLBACK_CONTROLLER_COMMANDS = [
    {
      label: "steering",
      payload_type: "STEERING_CONTINUOUS_COMMAND",
      payload_type_id: 4,
      fields: [{ name: "desired_wheel_angle_in_deg", type: "float", default: 0 }],
    },
    {
      label: "throttle",
      payload_type: "THROTTLE_CONTINUOUS_COMMAND",
      payload_type_id: 5,
      fields: [
        { name: "desired_throttle_in_percentage", type: "uint8_t", default: 0 },
        { name: "desired_rpm", type: "uint16_t", default: 0 },
      ],
    },
    {
      label: "brakes",
      payload_type: "BRAKES_CONTINUOUS_COMMAND",
      payload_type_id: 42,
      fields: [
        { name: "brake_mode", type: "BrakeMode", default: 0 },
        { name: "desired_brakes_position_in_percentage", type: "uint8_t", default: 0 },
      ],
    },
    {
      label: "reverser",
      payload_type: "REVERSER_COMMAND",
      payload_type_id: 41,
      fields: [{ name: "reverser_gear_mode", type: "ReverserGearMode", default: "REVERSER_GEAR_MODE_NEUTRAL" }],
    },
    {
      label: "power",
      payload_type: "POWER_COMMAND",
      payload_type_id: 98,
      fields: [{ name: "is_desired_control_on", type: "bool", default: true }],
    },
  ];

  let usbMicroOps = [];
  let usbControllerCmds = [...FALLBACK_CONTROLLER_COMMANDS];
  let usbOpened = false;

  function setUsbUi() {
    if (btnUsbOpen) btnUsbOpen.disabled = usbOpened;
    if (btnUsbClose) btnUsbClose.disabled = !usbOpened;
    if (btnUsbSend) btnUsbSend.disabled = !usbOpened;
    if (btnUsbSendController) btnUsbSendController.disabled = !usbOpened;
    if (usbPortEl) usbPortEl.disabled = usbOpened;
    if (usbOpened) {
      usbStatusEl.textContent = `Open: ${usbPortEl.value}`;
      usbStatusEl.classList.add("open");
    } else {
      usbStatusEl.textContent = "Closed";
      usbStatusEl.classList.remove("open");
    }
  }

  async function refreshUsbPorts() {
    const res = await fetch("/api/usb/ports");
    const data = await res.json();
    const prev = usbPortEl.value;
    usbPortEl.innerHTML = "";
    const ports = data.ports || [];
    if (ports.length === 0) {
      const opt = document.createElement("option");
      opt.value = "";
      opt.textContent = "(no ports found)";
      usbPortEl.appendChild(opt);
    } else {
      ports.forEach((p) => {
        const opt = document.createElement("option");
        opt.value = p;
        opt.textContent = p;
        usbPortEl.appendChild(opt);
      });
      if (prev && ports.includes(prev)) {
        usbPortEl.value = prev;
      }
    }
  }

  function renderUsbMicroFields(op) {
    usbMicroFieldsEl.innerHTML = "";
    if (!op) return;
    op.fields.forEach((f) => {
      const wrap = document.createElement("div");
      wrap.className = "field";
      const label = document.createElement("label");
      label.textContent = f.name;
      label.htmlFor = `usb-field-${f.name}`;
      const input = document.createElement("input");
      input.id = `usb-field-${f.name}`;
      input.dataset.field = f.name;
      if (f.array_size) {
        input.classList.add("wide");
        input.placeholder = `comma-separated (${f.array_size})`;
        if (Array.isArray(f.default)) {
          input.value = f.default.join(",");
        }
      } else {
        input.value =
          f.default !== undefined && f.default !== null ? String(f.default) : "0";
      }
      wrap.appendChild(label);
      wrap.appendChild(input);
      usbMicroFieldsEl.appendChild(wrap);
    });
  }

  function collectUsbFieldValues(op) {
    const values = {};
    op.fields.forEach((f) => {
      const input = document.getElementById(`usb-field-${f.name}`);
      if (!input) return;
      let raw = input.value.trim();
      if (f.array_size) {
        values[f.name] = raw
          ? raw.split(",").map((s) => s.trim())
          : [];
        return;
      }
      if (/^0x[0-9a-fA-F]+$/.test(raw)) {
        values[f.name] = raw;
      } else if (/^-?\d+$/.test(raw)) {
        values[f.name] = parseInt(raw, 10);
      } else {
        values[f.name] = raw;
      }
    });
    return values;
  }

  function renderUsbControllerFields(cmd) {
    if (!usbControllerFieldsEl) return;
    usbControllerFieldsEl.innerHTML = "";
    if (!cmd) return;
    cmd.fields.forEach((f) => {
      const wrap = document.createElement("div");
      wrap.className = "field";
      const label = document.createElement("label");
      label.textContent = f.name;
      label.htmlFor = `usb-ctrl-field-${f.name}`;
      const input = document.createElement("input");
      input.id = `usb-ctrl-field-${f.name}`;
      input.dataset.field = f.name;
      if (f.array_size) {
        input.classList.add("wide");
        input.placeholder = `comma-separated (${f.array_size})`;
        if (Array.isArray(f.default)) {
          input.value = f.default.join(",");
        }
      } else {
        input.value =
          f.default !== undefined && f.default !== null ? String(f.default) : "0";
      }
      wrap.appendChild(label);
      wrap.appendChild(input);
      usbControllerFieldsEl.appendChild(wrap);
    });
  }

  function collectUsbControllerValues(cmd) {
    const values = {};
    cmd.fields.forEach((f) => {
      const input = document.getElementById(`usb-ctrl-field-${f.name}`);
      if (!input) return;
      let raw = input.value.trim();
      if (f.array_size) {
        values[f.name] = raw ? raw.split(",").map((s) => s.trim()) : [];
        return;
      }
      if (/^0x[0-9a-fA-F]+$/.test(raw)) {
        values[f.name] = raw;
      } else if (/^-?\d+$/.test(raw)) {
        values[f.name] = parseInt(raw, 10);
      } else {
        values[f.name] = raw;
      }
    });
    return values;
  }

  async function loadUsbMicroOps() {
    usbMicroOpEl.innerHTML = "";
    const loading = document.createElement("option");
    loading.value = "";
    loading.textContent = "(loading micro commands…)";
    usbMicroOpEl.appendChild(loading);
    try {
      const res = await fetch("/api/usb/micro-ops");
      const data = await res.json();
      usbMicroOps = data.micro_ops || [];
    } catch (_e) {
      usbMicroOps = [];
    }
    usbMicroOpEl.innerHTML = "";
    if (!usbMicroOps.length) {
      const opt = document.createElement("option");
      opt.value = "";
      opt.textContent = "(no micro commands available)";
      usbMicroOpEl.appendChild(opt);
      usbMicroFieldsEl.innerHTML = "";
      return;
    }
    usbMicroOps.forEach((op) => {
      const opt = document.createElement("option");
      opt.value = op.union_member;
      opt.textContent = op.payload_type;
      opt.title = `id ${op.payload_type_id}`;
      usbMicroOpEl.appendChild(opt);
    });
    renderUsbMicroFields(usbMicroOps[0]);
  }

  function applyControllerCatalog(cmds) {
    if (!usbControllerCmdEl || !cmds.length) return;
    const prev = usbControllerCmdEl.value;
    usbControllerCmds = cmds;
    usbControllerCmdEl.innerHTML = "";
    cmds.forEach((cmd) => {
      const opt = document.createElement("option");
      opt.value = cmd.payload_type;
      opt.textContent = cmd.payload_type;
      opt.title = `id ${cmd.payload_type_id}`;
      usbControllerCmdEl.appendChild(opt);
    });
    const stillValid = cmds.some((c) => c.payload_type === prev);
    usbControllerCmdEl.value = stillValid ? prev : cmds[0].payload_type;
    const selected = cmds.find((c) => c.payload_type === usbControllerCmdEl.value) || cmds[0];
    renderUsbControllerFields(selected);
  }

  function initControllerCommandsFromDom() {
    if (!usbControllerCmdEl) return;
    const fromDom = [];
    for (const opt of usbControllerCmdEl.options) {
      if (!opt.value) continue;
      const fb = FALLBACK_CONTROLLER_COMMANDS.find((c) => c.payload_type === opt.value);
      if (fb) fromDom.push(fb);
    }
    if (fromDom.length) {
      applyControllerCatalog(fromDom);
    } else {
      applyControllerCatalog(FALLBACK_CONTROLLER_COMMANDS);
    }
  }

  async function loadUsbControllerCommands() {
    try {
      const res = await fetch("/api/usb/controller-commands");
      if (!res.ok) return;
      const data = await res.json();
      const cmds = data.controller_commands || [];
      if (cmds.length) applyControllerCatalog(cmds);
    } catch (_e) {
      /* keep built-in / HTML catalog */
    }
  }

  usbMicroOpEl.addEventListener("change", () => {
    const op = usbMicroOps.find((o) => o.union_member === usbMicroOpEl.value);
    renderUsbMicroFields(op);
  });

  if (usbControllerCmdEl) {
    initControllerCommandsFromDom();
    usbControllerCmdEl.addEventListener("change", () => {
      const cmd = usbControllerCmds.find((c) => c.payload_type === usbControllerCmdEl.value);
      renderUsbControllerFields(cmd);
    });
  }

  document.getElementById("btn-usb-refresh").addEventListener("click", refreshUsbPorts);

  btnUsbOpen.addEventListener("click", async () => {
    const port = usbPortEl.value;
    if (!port) {
      statusEl.textContent = "Select a USB port first";
      return;
    }
    statusEl.textContent = "Opening USB…";
    const res = await fetch("/api/usb/open", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ port }),
    });
    const data = await res.json();
    if (data.ok) {
      usbOpened = true;
      setUsbUi();
      statusEl.textContent = `USB open on ${port}`;
    } else {
      statusEl.textContent = `USB open failed: ${data.error}`;
    }
  });

  btnUsbClose.addEventListener("click", async () => {
    const res = await fetch("/api/usb/close", { method: "POST" });
    const data = await res.json();
    if (data.ok) usbOpened = false;
    setUsbUi();
    statusEl.textContent = data.ok ? "USB closed" : `USB close failed: ${data.error}`;
  });

  btnUsbSend.addEventListener("click", async () => {
    const op = usbMicroOps.find((o) => o.union_member === usbMicroOpEl.value);
    if (!op) return;
    const values = collectUsbFieldValues(op);
    statusEl.textContent = "Sending micro command…";
    btnUsbSend.disabled = true;
    const res = await fetch("/api/usb/send-micro", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        union_member: op.union_member,
        values,
        qos: "none",
      }),
    });
    const data = await res.json();
    btnUsbSend.disabled = !usbOpened;
    if (data.ok) {
      statusEl.textContent = `Sent ${data.payload_type} (${data.payload_hex})`;
      if (data.output) {
        term.writeln(`[USB] ${data.output}`);
      }
    } else {
      statusEl.textContent = `Send failed: ${data.error}`;
    }
  });

  btnUsbSendController.addEventListener("click", async () => {
    const cmd = usbControllerCmds.find((c) => c.payload_type === usbControllerCmdEl.value);
    if (!cmd) return;
    const values = collectUsbControllerValues(cmd);
    statusEl.textContent = "Sending controller command…";
    btnUsbSendController.disabled = true;
    const res = await fetch("/api/usb/send-controller", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        payload_type: cmd.payload_type,
        values,
        qos: "none",
      }),
    });
    const data = await res.json();
    btnUsbSendController.disabled = !usbOpened;
    if (data.ok) {
      statusEl.textContent = `Sent ${data.payload_type} (${data.payload_hex})`;
      if (data.output) {
        term.writeln(`[USB] ${data.output}`);
      }
    } else {
      statusEl.textContent = `Send failed: ${data.error}`;
    }
  });

  (async function initUsb() {
    loadUsbControllerCommands();
    loadUsbMicroOps();
    try {
      await refreshUsbPorts();
    } catch (_e) {
      /* port list optional */
    }
    try {
      const st = await fetch("/api/usb/status");
      const stData = await st.json();
      usbOpened = !!stData.opened;
      if (stData.port && usbPortEl) usbPortEl.value = stData.port;
    } catch (_e) {
      usbOpened = false;
    }
    setUsbUi();
  })();

  connect();
})();

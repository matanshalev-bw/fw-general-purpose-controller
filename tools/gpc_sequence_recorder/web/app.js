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

  // --- USB immediate micro commands ---
  const usbPortEl = document.getElementById("usb-port");
  const usbStatusEl = document.getElementById("usb-status");
  const usbMicroOpEl = document.getElementById("usb-micro-op");
  const usbMicroFieldsEl = document.getElementById("usb-micro-fields");
  const btnUsbOpen = document.getElementById("btn-usb-open");
  const btnUsbClose = document.getElementById("btn-usb-close");
  const btnUsbSend = document.getElementById("btn-usb-send");

  let usbMicroOps = [];
  let usbOpened = false;

  function setUsbUi() {
    btnUsbOpen.disabled = usbOpened;
    btnUsbClose.disabled = !usbOpened;
    btnUsbSend.disabled = !usbOpened;
    usbPortEl.disabled = usbOpened;
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

  async function loadUsbMicroOps() {
    const res = await fetch("/api/usb/micro-ops");
    const data = await res.json();
    usbMicroOps = data.micro_ops || [];
    usbMicroOpEl.innerHTML = "";
    usbMicroOps.forEach((op) => {
      const opt = document.createElement("option");
      opt.value = op.union_member;
      opt.textContent = `${op.union_member} (${op.payload_type})`;
      usbMicroOpEl.appendChild(opt);
    });
    if (usbMicroOps.length) {
      renderUsbMicroFields(usbMicroOps[0]);
    }
  }

  usbMicroOpEl.addEventListener("change", () => {
    const op = usbMicroOps.find((o) => o.union_member === usbMicroOpEl.value);
    renderUsbMicroFields(op);
  });

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

  (async function initUsb() {
    await refreshUsbPorts();
    await loadUsbMicroOps();
    const st = await fetch("/api/usb/status");
    const stData = await st.json();
    usbOpened = !!stData.opened;
    if (stData.port) usbPortEl.value = stData.port;
    setUsbUi();
  })();

  connect();
})();

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

  const usbLogTerm = new Terminal({
    theme: { background: "#1a1d1e", foreground: "#cccccc", cursor: "#666666" },
    fontFamily: "Consolas, Monaco, monospace",
    fontSize: 12,
    cursorBlink: false,
    disableStdin: true,
  });
  const usbLogFitAddon = new FitAddon.FitAddon();
  usbLogTerm.loadAddon(usbLogFitAddon);
  usbLogTerm.open(document.getElementById("usb-log-terminal"));
  usbLogFitAddon.fit();

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

  /** Piped subprocess stdout uses LF-only newlines; xterm needs CRLF for column reset. */
  function writeProcessOutput(text) {
    term.write(text.replace(/\r\n/g, "\n").replace(/\n/g, "\r\n"));
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

  window.addEventListener("resize", () => {
    fitAddon.fit();
    usbLogFitAddon.fit();
  });

  function flashConfigViaUsb(port) {
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
        term.writeln(`[Flash] Starting programmer on ${port}…`);
        flashWs.send(JSON.stringify({ port }));
      };

      flashWs.onmessage = (ev) => {
        const msg = JSON.parse(ev.data);
        if (msg.type === "output" && msg.text) {
          writeProcessOutput(msg.text);
          return;
        }
        if (msg.type === "done") {
          term.writeln("");
          flashWs.close();
          finish(resolve, msg);
          return;
        }
        if (msg.type === "error") {
          term.writeln("");
          term.writeln(`[Flash] ${msg.message || "Flash failed"}`);
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

  document.getElementById("btn-flash").addEventListener("click", async () => {
    const port = document.getElementById("usb-port")?.value || "";
    const binPath = "config_projects/config_g474/Debug/config_g474.bin";
    if (!port) {
      statusEl.textContent = "Select a USB port first";
      return;
    }
    if (
      !confirm(
        `Flash config to ${port} via USB?\n\nUses ${binPath} (built with Export).`
      )
    ) {
      return;
    }
    const btnFlash = document.getElementById("btn-flash");
    btnFlash.disabled = true;
    statusEl.textContent = "Flashing config…";
    try {
      const data = await flashConfigViaUsb(port);
      statusEl.textContent = `Flashed ${data.bin_path} to ${data.port}`;
    } catch (e) {
      statusEl.textContent = `Flash failed: ${e.message}`;
    } finally {
      btnFlash.disabled = false;
    }
  });

  document.getElementById("btn-export").addEventListener("click", async () => {
    const path = "configs/ConfigsTypes/g474_gpc_config_memory.hpp";
    const binPath = "config_projects/config_g474/Debug/config_g474.bin";
    if (
      !confirm(
        `Export to ${path} and ${binPath}? This will overwrite both files.`
      )
    ) {
      return;
    }
    statusEl.textContent = "Exporting…";
    const res = await fetch("/api/export", { method: "POST" });
    const data = await res.json();
    if (data.ok) {
      statusEl.textContent = `Exported to ${data.path} and ${data.bin_path}. ${data.flash_note || ""}`;
      previewEl.textContent = data.hpp;
    } else {
      statusEl.textContent = `Export failed: ${data.error}`;
    }
  });

  document.getElementById("btn-reload").addEventListener("click", () => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      statusEl.textContent = "Not connected — refresh to reconnect";
      return;
    }
    term.write("\r\n" + prompt + "reload()\r\n");
    ws.send("reload()");
    lineBuffer = "";
  });

  document.getElementById("btn-help").addEventListener("click", () => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send("help()");
    }
  });

  // --- bluelink struct dictionary ---
  const dictOverlay = document.getElementById("dict-overlay");
  const dictBody = document.getElementById("dict-body");
  const dictFilter = document.getElementById("dict-filter");
  const btnDictTabBluelink = document.getElementById("btn-dict-tab-bluelink");
  const btnDictTabRecorder = document.getElementById("btn-dict-tab-recorder");
  const btnDictTabLlcStates = document.getElementById("btn-dict-tab-llc-states");
  let dictBluelinkData = null;
  let dictRecorderData = null;
  let dictMode = "bluelink"; // 'bluelink' | 'recorder' | 'llc-states'

  const LLC_STATE_MACHINE_DOC_URL =
    "https://bw-robotics.atlassian.net/wiki/spaces/EmbTeam/pages/1019772976/Gen+4.0+State+Machine+Architecture+Document";

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

  function renderStateMachineDictionary() {
    if (!dictBody) return;
    dictBody.innerHTML = `<div class="dict-section dict-state-machine">
      <h3>Gen 4.0 LLC state machine (bluelink::LlcStates)</h3>
      <img src="/assets/gen4_llc_state_machine.png" alt="Gen 4.0 LLC System bluelink::LlcStates state machine diagram" />
      <div class="caption">
        Reference for bind_state / bind_state_tick controller states and LLC transitions.<br />
        <a href="${LLC_STATE_MACHINE_DOC_URL}" target="_blank" rel="noopener noreferrer">Gen 4.0 State Machine Architecture Document</a>
      </div>
    </div>`;
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

  function renderRecorderDictionary(data, filter) {
    if (!dictBody) return;
    const q = (filter || "").trim().toLowerCase();
    const match = (text) => !q || text.includes(q);
    const cmds = (data.recorder_commands || [])
      .map((c) => {
        const title = `${c.name}${c.signature || "()"}`;
        const meta = "Recorder DSL builtin";
        const desc = c.description || "";
        const search = `${title} ${desc} ${c.doc || ""} ${c.example || ""}`.toLowerCase();
        return { title, meta, desc, doc: c.doc || "", example: c.example || "", search };
      })
      .filter((e) => match(e.search));

    let html = '<div class="dict-section"><h3>Recorder commands (REPL)</h3>';
    if (!cmds.length) {
      html += '<p class="dict-empty">No matching commands.</p>';
    } else {
      html += cmds
        .map((c) => {
          const descHtml = c.desc ? `<div class="field">${c.desc}</div>` : '<div class="field dict-empty">(no description)</div>';
          const docHtml = c.doc ? `<div class="field">${c.doc}</div>` : "";
          const exHtml = c.example
            ? `<div class="field"><span class="type">example</span> <span class="def">${c.example}</span></div>`
            : "";
          return `<div class="dict-entry">
            <div class="title">${c.title}</div>
            <div class="meta">${c.meta}</div>
            ${descHtml}
            ${docHtml}
            ${exHtml}
          </div>`;
        })
        .join("");
    }
    html += "</div>";
    dictBody.innerHTML = html;
  }

  function setDictMode(mode) {
    dictMode = mode;
    btnDictTabBluelink?.classList.toggle("active", mode === "bluelink");
    btnDictTabRecorder?.classList.toggle("active", mode === "recorder");
    btnDictTabLlcStates?.classList.toggle("active", mode === "llc-states");
    if (dictFilter) {
      dictFilter.style.display = mode === "llc-states" ? "none" : "";
    }
    const filter = dictFilter ? dictFilter.value : "";
    if (mode === "llc-states") {
      renderStateMachineDictionary();
    } else if (mode === "recorder") {
      if (dictRecorderData) renderRecorderDictionary(dictRecorderData, filter);
      else loadRecorderDictionary();
    } else {
      if (dictBluelinkData) renderDictionary(dictBluelinkData, filter);
      else loadBluelinkDictionary();
    }
  }

  async function loadBluelinkDictionary() {
    if (dictBluelinkData) {
      renderDictionary(dictBluelinkData, dictFilter ? dictFilter.value : "");
      return;
    }
    if (dictBody) dictBody.innerHTML = '<p class="dict-empty">Loading…</p>';
    try {
      const res = await fetch("/api/schema/commands-dictionary");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      dictBluelinkData = await res.json();
      if (dictMode === "bluelink") {
        renderDictionary(dictBluelinkData, dictFilter ? dictFilter.value : "");
      }
    } catch (e) {
      if (dictBody) {
        dictBody.innerHTML = `<p class="dict-empty">Failed to load dictionary: ${e.message}</p>`;
      }
    }
  }

  async function loadRecorderDictionary() {
    if (dictRecorderData) {
      renderRecorderDictionary(dictRecorderData, dictFilter ? dictFilter.value : "");
      return;
    }
    if (dictBody) dictBody.innerHTML = '<p class="dict-empty">Loading…</p>';
    try {
      const res = await fetch("/api/schema/recorder-dictionary");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      dictRecorderData = await res.json();
      if (dictMode === "recorder") {
        renderRecorderDictionary(dictRecorderData, dictFilter ? dictFilter.value : "");
      }
    } catch (e) {
      if (dictBody) {
        dictBody.innerHTML = `<p class="dict-empty">Failed to load recorder dictionary: ${e.message}</p>`;
      }
    }
  }

  function openDictionary() {
    if (!dictOverlay) return;
    dictOverlay.classList.add("open");
    setDictMode(dictMode);
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
  btnDictTabBluelink?.addEventListener("click", () => setDictMode("bluelink"));
  btnDictTabRecorder?.addEventListener("click", () => setDictMode("recorder"));
  btnDictTabLlcStates?.addEventListener("click", () => setDictMode("llc-states"));
  dictOverlay?.addEventListener("click", (e) => {
    if (e.target === dictOverlay) closeDictionary();
  });
  dictFilter?.addEventListener("input", () => {
    const filter = dictFilter.value;
    if (dictMode === "recorder") {
      if (dictRecorderData) renderRecorderDictionary(dictRecorderData, filter);
    } else if (dictMode === "bluelink") {
      if (dictBluelinkData) renderDictionary(dictBluelinkData, filter);
    }
  });
  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape" && dictOverlay?.classList.contains("open")) {
      closeDictionary();
    }
  });

  // --- Recorder command bar (insert into terminal) ---
  const recorderCmdEl = document.getElementById("recorder-cmd");
  const recorderCmdTooltipEl = document.getElementById("recorder-cmd-tooltip");
  const recorderFieldsEl = document.getElementById("recorder-fields");
  const btnRecorderInsert = document.getElementById("btn-recorder-insert");
  let recorderCmds = [];
  let recorderCmdTooltipHover = false;
  let controllerOneShotStates = [];
  let controllerTickStates = [];
  let bindableCommands = []; // from /api/schema/commands-dictionary
  let bindableTelemetries = []; // from /api/schema/commands-dictionary (telemetries ≤8 bytes)

  function terminalInsert(text) {
    if (!text) return;
    const s = String(text);
    term.write(s);
    lineBuffer += s;
  }

  function hideRecorderCmdTooltip() {
    if (!recorderCmdTooltipEl) return;
    recorderCmdTooltipEl.hidden = true;
    recorderCmdTooltipEl.textContent = "";
  }

  function showRecorderCmdTooltip(anchor, description) {
    if (!recorderCmdTooltipEl || !description) {
      hideRecorderCmdTooltip();
      return;
    }
    recorderCmdTooltipEl.textContent = description;
    recorderCmdTooltipEl.hidden = false;
    const rect = anchor.getBoundingClientRect();
    recorderCmdTooltipEl.style.left = `${rect.left + rect.width / 2}px`;
    recorderCmdTooltipEl.style.top = `${rect.top - 8}px`;
    recorderCmdTooltipEl.style.transform = "translate(-50%, -100%)";
  }

  function recorderCmdDescription(name) {
    const cmd = recorderCmds.find((c) => c.name === name);
    return cmd?.description || "";
  }

  function updateRecorderCmdTooltip() {
    if (!recorderCmdTooltipHover || !recorderCmdEl) return;
    showRecorderCmdTooltip(recorderCmdEl, recorderCmdDescription(recorderCmdEl.value));
  }

  function setRecorderCmdSelect(cmds) {
    if (!recorderCmdEl) return;
    recorderCmdEl.innerHTML = "";
    let currentGroup = null;
    let groupEl = null;
    cmds.forEach((c) => {
      const group = c.group || "";
      if (group !== currentGroup) {
        currentGroup = group;
        if (group) {
          groupEl = document.createElement("optgroup");
          groupEl.label = group;
          recorderCmdEl.appendChild(groupEl);
        } else {
          groupEl = null;
        }
      }
      const opt = document.createElement("option");
      opt.value = c.name;
      opt.textContent = c.name;
      opt.title = c.description || "";
      opt.dataset.description = c.description || "";
      (groupEl || recorderCmdEl).appendChild(opt);
    });
  }

  recorderCmdEl?.addEventListener("mouseenter", () => {
    recorderCmdTooltipHover = true;
    updateRecorderCmdTooltip();
  });
  recorderCmdEl?.addEventListener("mouseleave", () => {
    recorderCmdTooltipHover = false;
    hideRecorderCmdTooltip();
  });
  recorderCmdEl?.addEventListener("input", updateRecorderCmdTooltip);
  recorderCmdEl?.addEventListener("focus", () => {
    recorderCmdTooltipHover = true;
    updateRecorderCmdTooltip();
  });
  recorderCmdEl?.addEventListener("blur", () => {
    recorderCmdTooltipHover = false;
    hideRecorderCmdTooltip();
  });

  function isListParam(p) {
    if (!p) return false;
    if (p.is_list) return true;
    const ann = String(p.annotation || "");
    return ann === "List" || ann.includes("List[") || ann.includes("list[");
  }

  function renderRecorderFields(cmd) {
    if (!recorderFieldsEl) return;
    recorderFieldsEl.innerHTML = "";
    if (!cmd || !cmd.params) return;

    if (cmd.name === "bind_command") {
      const triggerWrap = document.createElement("div");
      triggerWrap.className = "field";
      const triggerLabel = document.createElement("label");
      triggerLabel.textContent = "trigger";
      triggerLabel.htmlFor = "rec-bind-trigger";
      const triggerSelect = document.createElement("select");
      triggerSelect.id = "rec-bind-trigger";
      triggerSelect.dataset.param = "trigger";
      triggerSelect.title = "PayloadTypeIds trigger (bindable)";

      const bindables = (bindableCommands || []).filter((c) => !!c.payload_type);
      if (!bindables.length) {
        const opt = document.createElement("option");
        opt.value = "";
        opt.textContent = "(no triggers available)";
        triggerSelect.appendChild(opt);
      } else {
        bindables.forEach((c) => {
          const opt = document.createElement("option");
          opt.value = c.payload_type;
          opt.textContent = c.payload_type;
          triggerSelect.appendChild(opt);
        });
      }

      triggerWrap.appendChild(triggerLabel);
      triggerWrap.appendChild(triggerSelect);
      recorderFieldsEl.appendChild(triggerWrap);

      const structFieldsWrap = document.createElement("div");
      structFieldsWrap.className = "field";
      const structLabel = document.createElement("label");
      structLabel.textContent = "struct";
      structLabel.htmlFor = "rec-bind-struct";
      const structName = document.createElement("input");
      structName.id = "rec-bind-struct";
      structName.disabled = true;
      structName.classList.add("wide");
      structFieldsWrap.appendChild(structLabel);
      structFieldsWrap.appendChild(structName);
      recorderFieldsEl.appendChild(structFieldsWrap);

      const fieldsContainer = document.createElement("div");
      fieldsContainer.id = "rec-bind-fields";
      fieldsContainer.style.display = "flex";
      fieldsContainer.style.flexWrap = "wrap";
      fieldsContainer.style.gap = "0.35rem 0.75rem";
      fieldsContainer.style.alignItems = "center";
      recorderFieldsEl.appendChild(fieldsContainer);

      const renderStructFields = () => {
        fieldsContainer.innerHTML = "";
        const selected = bindables.find((c) => c.payload_type === triggerSelect.value) || bindables[0];
        if (!selected) return;
        structName.value = selected.struct_name || "";
        (selected.fields || []).forEach((f) => {
          const wrap = document.createElement("div");
          wrap.className = "field";
          const label = document.createElement("label");
          label.textContent = f.name;
          label.htmlFor = `rec-bind-field-${f.name}`;
          let input;
          if (f.enum_values && f.enum_values.length) {
            input = document.createElement("select");
            input.id = `rec-bind-field-${f.name}`;
            f.enum_values.forEach((ev) => {
              const opt = document.createElement("option");
              opt.value = ev.name;
              opt.textContent = ev.name;
              input.appendChild(opt);
            });
            if (f.default !== undefined && f.default !== null) {
              input.value = String(f.default);
            }
          } else {
            input = document.createElement("input");
            input.id = `rec-bind-field-${f.name}`;
            if (f.array_size) {
              input.classList.add("wide");
              input.placeholder = `comma-separated (${f.array_size})`;
              if (Array.isArray(f.default)) input.value = f.default.join(",");
            } else if (f.default !== undefined && f.default !== null) {
              input.value = String(f.default);
            }
          }
          input.dataset.field = f.name;
          input.dataset.type = f.type || "";
          wrap.appendChild(label);
          wrap.appendChild(input);
          fieldsContainer.appendChild(wrap);
        });
      };

      triggerSelect.addEventListener("change", renderStructFields);
      renderStructFields();
      return;
    }

    if (cmd.name === "bind_telemetry") {
      const rateWrap = document.createElement("div");
      rateWrap.className = "field";
      const rateLabel = document.createElement("label");
      rateLabel.textContent = "rate";
      rateLabel.htmlFor = "rec-telemetry-rate";
      const rateInput = document.createElement("input");
      rateInput.id = "rec-telemetry-rate";
      rateInput.type = "number";
      rateInput.min = "1";
      rateInput.value = "1";
      rateInput.dataset.param = "rate";
      rateWrap.appendChild(rateLabel);
      rateWrap.appendChild(rateInput);
      recorderFieldsEl.appendChild(rateWrap);

      const triggerWrap = document.createElement("div");
      triggerWrap.className = "field";
      const triggerLabel = document.createElement("label");
      triggerLabel.textContent = "trigger";
      triggerLabel.htmlFor = "rec-telemetry-trigger";
      const triggerSelect = document.createElement("select");
      triggerSelect.id = "rec-telemetry-trigger";
      triggerSelect.dataset.param = "trigger";
      triggerSelect.title = "Telemetry PayloadTypeIds (≤8 bytes)";

      const bindables = (bindableTelemetries || []).filter((t) => !!t.payload_type);
      if (!bindables.length) {
        const opt = document.createElement("option");
        opt.value = "";
        opt.textContent = "(no telemetry types available)";
        triggerSelect.appendChild(opt);
      } else {
        bindables.forEach((t) => {
          const opt = document.createElement("option");
          opt.value = t.payload_type;
          opt.textContent = `${t.payload_type} (${t.payload_size}B)`;
          triggerSelect.appendChild(opt);
        });
      }

      triggerWrap.appendChild(triggerLabel);
      triggerWrap.appendChild(triggerSelect);
      recorderFieldsEl.appendChild(triggerWrap);

      const structFieldsWrap = document.createElement("div");
      structFieldsWrap.className = "field";
      const structLabel = document.createElement("label");
      structLabel.textContent = "struct";
      structLabel.htmlFor = "rec-telemetry-struct";
      const structName = document.createElement("input");
      structName.id = "rec-telemetry-struct";
      structName.disabled = true;
      structName.classList.add("wide");
      structFieldsWrap.appendChild(structLabel);
      structFieldsWrap.appendChild(structName);
      recorderFieldsEl.appendChild(structFieldsWrap);

      const fieldsContainer = document.createElement("div");
      fieldsContainer.id = "rec-telemetry-fields";
      fieldsContainer.style.display = "flex";
      fieldsContainer.style.flexWrap = "wrap";
      fieldsContainer.style.gap = "0.35rem 0.75rem";
      fieldsContainer.style.alignItems = "center";
      recorderFieldsEl.appendChild(fieldsContainer);

      const renderTelemetryFields = () => {
        fieldsContainer.innerHTML = "";
        const selected = bindables.find((t) => t.payload_type === triggerSelect.value) || bindables[0];
        if (!selected) return;
        structName.value = selected.struct_name || "";
        (selected.fields || []).forEach((f) => {
          const wrap = document.createElement("div");
          wrap.className = "field";
          const label = document.createElement("label");
          const paramName = `${f.name}_var_index`;
          label.textContent = paramName;
          label.htmlFor = `rec-telemetry-field-${f.name}`;
          const input = document.createElement("input");
          input.id = `rec-telemetry-field-${f.name}`;
          input.type = "number";
          input.min = "0";
          input.max = "7";
          input.value = "0";
          input.dataset.field = f.name;
          input.dataset.param = paramName;
          wrap.appendChild(label);
          wrap.appendChild(input);
          fieldsContainer.appendChild(wrap);
        });
      };

      triggerSelect.addEventListener("change", renderTelemetryFields);
      renderTelemetryFields();
      return;
    }

    if (cmd.name === "bind_state" || cmd.name === "clear_state") {
      const stateWrap = document.createElement("div");
      stateWrap.className = "field";
      const stateLabel = document.createElement("label");
      stateLabel.textContent = "state";
      stateLabel.htmlFor = "rec-state-select";
      const stateSelect = document.createElement("select");
      stateSelect.id = "rec-state-select";
      stateSelect.dataset.param = "state";
      const states = controllerOneShotStates.length
        ? controllerOneShotStates
        : ["CONTROLLER_STATE_INIT"];
      states.forEach((s) => {
        const opt = document.createElement("option");
        opt.value = s;
        opt.textContent = s;
        stateSelect.appendChild(opt);
      });
      stateWrap.appendChild(stateLabel);
      stateWrap.appendChild(stateSelect);
      recorderFieldsEl.appendChild(stateWrap);
      return;
    }

    if (cmd.name === "bind_state_tick" || cmd.name === "clear_state_tick") {
      const stateWrap = document.createElement("div");
      stateWrap.className = "field";
      const stateLabel = document.createElement("label");
      stateLabel.textContent = "state";
      stateLabel.htmlFor = "rec-state-select";
      const stateSelect = document.createElement("select");
      stateSelect.id = "rec-state-select";
      stateSelect.dataset.param = "state";
      const states = controllerTickStates.length
        ? controllerTickStates
        : ["CONTROLLER_STATE_OPERATIONAL"];
      states.forEach((s) => {
        const opt = document.createElement("option");
        opt.value = s;
        opt.textContent = s;
        stateSelect.appendChild(opt);
      });
      stateWrap.appendChild(stateLabel);
      stateWrap.appendChild(stateSelect);
      recorderFieldsEl.appendChild(stateWrap);
      return;
    }

    cmd.params.forEach((p) => {
      if (p.kind === "VAR_KEYWORD" || p.kind === "VAR_POSITIONAL") return;
      const wrap = document.createElement("div");
      wrap.className = "field";
      const label = document.createElement("label");
      const input = document.createElement("input");
      input.id = `rec-field-${p.name}`;
      input.dataset.param = p.name;
      input.dataset.annotation = p.annotation || "";
      input.dataset.hasDefault = p.has_default ? "1" : "0";

      const isList = isListParam(p);
      label.textContent = isList ? `${p.name}[]` : p.name;
      label.htmlFor = `rec-field-${p.name}`;
      if (isList) {
        input.classList.add("wide");
        input.placeholder = "comma-separated";
      } else if (!p.has_default) {
        input.placeholder = "required";
      }
      if (p.has_default && p.default !== null && p.default !== undefined) {
        input.value = String(p.default);
      }

      wrap.appendChild(label);
      wrap.appendChild(input);
      recorderFieldsEl.appendChild(wrap);
    });
  }

  function pythonLiteral(raw) {
    const s = String(raw ?? "").trim();
    if (s === "") return "";
    if (s === "True" || s === "False" || s === "true" || s === "false") {
      return s[0] === "t" ? "True" : s[0] === "f" ? "False" : s;
    }
    if (/^0x[0-9a-fA-F]+$/.test(s) || /^-?\d+(\.\d+)?$/.test(s)) return s;
    if (/^[A-Za-z_][A-Za-z0-9_]*$/.test(s)) return s; // enum / constant token
    // quote as python string
    return JSON.stringify(s);
  }

  function pythonListLiteral(raw) {
    const s = String(raw ?? "").trim();
    if (!s) return "[]";
    const parts = s
      .split(",")
      .map((x) => x.trim())
      .filter(Boolean)
      .map(pythonLiteral);
    return `[${parts.join(", ")}]`;
  }

  function buildRecorderCall(cmd) {
    if (!cmd) return "";
    if (cmd.name === "bind_command") {
      const trigger = document.getElementById("rec-bind-trigger")?.value || "";
      if (!trigger) return "";
      const kwargs = [`trigger=${pythonLiteral(trigger)}`];

      const bindables = (bindableCommands || []).filter((c) => !!c.payload_type);
      const selected = bindables.find((c) => c.payload_type === trigger);
      if (selected) {
        (selected.fields || []).forEach((f) => {
          const input = document.getElementById(`rec-bind-field-${f.name}`);
          const raw = input ? input.value.trim() : "";
          const lit = f.array_size ? pythonListLiteral(raw) : pythonLiteral(raw);
          if (lit !== "") {
            kwargs.push(`${f.name}=${lit}`);
          }
        });
      }
      return `bind_command(${kwargs.join(", ")})`;
    }
    if (cmd.name === "bind_telemetry") {
      const rate = document.getElementById("rec-telemetry-rate")?.value || "";
      const trigger = document.getElementById("rec-telemetry-trigger")?.value || "";
      if (!rate || !trigger) return "";
      const kwargs = [`rate=${pythonLiteral(rate)}`, `trigger=${pythonLiteral(trigger)}`];

      const bindables = (bindableTelemetries || []).filter((t) => !!t.payload_type);
      const selected = bindables.find((t) => t.payload_type === trigger);
      if (selected) {
        (selected.fields || []).forEach((f) => {
          const input = document.getElementById(`rec-telemetry-field-${f.name}`);
          const raw = input ? input.value.trim() : "";
          if (raw !== "") {
            kwargs.push(`${f.name}_var_index=${pythonLiteral(raw)}`);
          }
        });
      }
      return `bind_telemetry(${kwargs.join(", ")})`;
    }
    if (cmd.name === "bind_state" || cmd.name === "clear_state" ||
        cmd.name === "bind_state_tick" || cmd.name === "clear_state_tick") {
      const state = document.getElementById("rec-state-select")?.value || "";
      if (!state) return "";
      return `${cmd.name}(${pythonLiteral(state)})`;
    }
    const params = cmd.params || [];
    const kwargs = [];

    params.forEach((p) => {
      if (p.kind === "VAR_KEYWORD" || p.kind === "VAR_POSITIONAL") return;
      const input = document.getElementById(`rec-field-${p.name}`);
      const raw = input ? input.value.trim() : "";
      const isEmpty = raw === "";
      if (isEmpty && p.has_default) return;
      if (isEmpty && !p.has_default) return; // required but missing

      const isList = isListParam(p);
      const lit = isList ? pythonListLiteral(raw) : pythonLiteral(raw);
      kwargs.push(`${p.name}=${lit}`);
    });

    const all = [...kwargs].filter(Boolean);
    return `${cmd.name}(${all.join(", ")})`;
  }

  async function loadRecorderCommandsForBar() {
    if (!recorderCmdEl) return;
    recorderCmdEl.innerHTML = "";
    const loading = document.createElement("option");
    loading.value = "";
    loading.textContent = "(loading recorder commands…)";
    recorderCmdEl.appendChild(loading);
    try {
      const res = await fetch("/api/schema/recorder-dictionary");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      recorderCmds = data.recorder_commands || [];
      controllerOneShotStates = data.controller_one_shot_states || [];
      controllerTickStates = data.controller_tick_states || [];
    } catch (_e) {
      recorderCmds = [];
    }
    recorderCmdEl.innerHTML = "";
    if (!recorderCmds.length) {
      const opt = document.createElement("option");
      opt.value = "";
      opt.textContent = "(no recorder commands available)";
      recorderCmdEl.appendChild(opt);
      if (btnRecorderInsert) btnRecorderInsert.disabled = true;
      return;
    }
    setRecorderCmdSelect(recorderCmds);
    renderRecorderFields(recorderCmds[0]);
    if (btnRecorderInsert) btnRecorderInsert.disabled = false;
  }

  async function loadBindableCommandsForRecorder() {
    try {
      const res = await fetch("/api/schema/commands-dictionary");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      bindableCommands = data.commands || [];
      bindableTelemetries = data.telemetries || [];
    } catch (_e) {
      bindableCommands = [];
      bindableTelemetries = [];
    }
  }

  recorderCmdEl?.addEventListener("change", () => {
    const cmd = recorderCmds.find((c) => c.name === recorderCmdEl.value);
    renderRecorderFields(cmd);
    updateRecorderCmdTooltip();
  });

  btnRecorderInsert?.addEventListener("click", () => {
    if (!recorderCmdEl) return;
    const name = recorderCmdEl.value;
    const cmd = recorderCmds.find((c) => c.name === name);
    if (!cmd) return;
    const call = buildRecorderCall(cmd);
    if (!call) return;
    if (ws && ws.readyState === WebSocket.OPEN) {
      term.write("\r\n" + prompt + call + "\r\n");
      ws.send(call.trim());
      lineBuffer = "";
    } else {
      terminalInsert(call);
    }
  });

  // --- USB immediate micro commands ---
  const usbPortEl = document.getElementById("usb-port");
  const usbStatusEl = document.getElementById("usb-status");
  const usbDestComponentEl = document.getElementById("usb-dest-component");
  const usbDestUnlockEl = document.getElementById("usb-dest-unlock");
  const usbMicroOpEl = document.getElementById("usb-micro-op");
  const usbMicroFieldsEl = document.getElementById("usb-micro-fields");
  const usbControllerCmdEl = document.getElementById("usb-controller-cmd");
  const usbControllerFieldsEl = document.getElementById("usb-controller-fields");
  const btnUsbOpen = document.getElementById("btn-usb-open");
  const btnUsbClose = document.getElementById("btn-usb-close");
  const btnUsbSend = document.getElementById("btn-usb-send");
  const btnUsbSendController = document.getElementById("btn-usb-send-controller");
  const btnUsbLogStart = document.getElementById("btn-usb-log-start");
  const btnUsbLogStop = document.getElementById("btn-usb-log-stop");
  const btnUsbLogClear = document.getElementById("btn-usb-log-clear");

  /** Built-in catalog so the destination dropdown works before/without API. */
  const FALLBACK_DESTINATION_COMPONENTS = [
    { name: "COMPONENT_ID_REVERSER_DRIVER", value: 0x0a },
    { name: "COMPONENT_ID_IMPLEMENT_DRIVER", value: 0x0b },
    { name: "COMPONENT_ID_POWER_PANEL_DRIVER", value: 0x0c },
    { name: "COMPONENT_ID_STEERING_DRIVER", value: 0x0d },
    { name: "COMPONENT_ID_REVERSER_AUX", value: 0x0e },
    { name: "COMPONENT_ID_POWER_PANEL_AUX", value: 0x0f },
    { name: "COMPONENT_ID_POWER_PANEL_TESTER", value: 0x10 },
    { name: "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER", value: 0x11 },
  ];

  /** Built-in catalog so the controller dropdown works before/without API. */
  const FALLBACK_CONTROLLER_COMMANDS = [
    {
      label: "controller_state",
      payload_type: "CONTROLLER_STATE_COMMAND",
      payload_type_id: 109,
      fields: [
        {
          name: "controller_state",
          type: "ControllerState",
          default: "CONTROLLER_STATE_INIT",
          enum_values: [
            "CONTROLLER_STATE_DISENGAGEMENT",
            "CONTROLLER_STATE_INIT",
            "CONTROLLER_STATE_ENGAGED",
            "CONTROLLER_STATE_POWER_UP_BIT",
          ],
        },
      ],
    },
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
  let usbLogRunning = false;
  let usbLogWs = null;

  function writeUsbLogOutput(text) {
    usbLogTerm.write(text.replace(/\r\n/g, "\n").replace(/\n/g, "\r\n"));
  }

  function updateUsbLogButtons() {
    if (btnUsbLogStart) btnUsbLogStart.disabled = !usbOpened || usbLogRunning;
    if (btnUsbLogStop) btnUsbLogStop.disabled = !usbLogRunning;
    if (btnUsbSend) btnUsbSend.disabled = !usbOpened;
    if (btnUsbSendController) btnUsbSendController.disabled = !usbOpened;
  }

  function stopUsbLogStream() {
    if (usbLogWs) {
      try {
        usbLogWs.close();
      } catch (_e) {
        /* ignore */
      }
      usbLogWs = null;
    }
    usbLogRunning = false;
    updateUsbLogButtons();
  }

  async function startUsbLogStream() {
    if (!usbOpened || usbLogRunning) return;
    const port = usbPortEl?.value || "";
    if (!port) {
      usbLogTerm.writeln("[log] Select a USB port first.");
      return;
    }

    const proto = location.protocol === "https:" ? "wss:" : "ws:";
    usbLogWs = new WebSocket(`${proto}//${location.host}/ws/usb-log`);
    usbLogRunning = true;
    updateUsbLogButtons();
    usbLogTerm.writeln(`[log] Listening on ${port}...`);

    usbLogWs.onopen = () => {
      usbLogWs.send(JSON.stringify({ port }));
    };
    usbLogWs.onmessage = (ev) => {
      const msg = JSON.parse(ev.data);
      if (msg.type === "output" && msg.text) {
        writeUsbLogOutput(msg.text);
      } else if (msg.type === "error") {
        usbLogTerm.writeln(`\r\n[log] Error: ${msg.message || "unknown"}`);
        stopUsbLogStream();
      } else if (msg.type === "done") {
        usbLogTerm.writeln("\r\n[log] Stopped.");
        stopUsbLogStream();
      }
    };
    usbLogWs.onclose = () => {
      if (usbLogRunning) {
        usbLogTerm.writeln("\r\n[log] Connection closed.");
      }
      stopUsbLogStream();
    };
    usbLogWs.onerror = () => {
      usbLogTerm.writeln("\r\n[log] WebSocket error.");
      stopUsbLogStream();
    };
  }

  function setUsbDestinationLocked(locked) {
    if (usbDestComponentEl) usbDestComponentEl.disabled = locked;
  }

  function selectedDestinationComponent() {
    return usbDestComponentEl ? usbDestComponentEl.value : "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER";
  }

  function applyDestinationComponentCatalog(items) {
    if (!usbDestComponentEl || !items.length) return;
    const prev = usbDestComponentEl.value;
    usbDestComponentEl.innerHTML = "";
    items.forEach((item) => {
      const opt = document.createElement("option");
      opt.value = item.name;
      opt.textContent = `${item.name} (0x${item.value.toString(16).toUpperCase().padStart(2, "0")})`;
      usbDestComponentEl.appendChild(opt);
    });
    const stillValid = items.some((item) => item.name === prev);
    usbDestComponentEl.value = stillValid
      ? prev
      : "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER";
  }

  async function loadUsbDestinationComponents() {
    applyDestinationComponentCatalog(FALLBACK_DESTINATION_COMPONENTS);
    try {
      const res = await fetch("/api/usb/component-ids");
      if (!res.ok) return;
      const data = await res.json();
      const items = data.component_ids || [];
      if (items.length) applyDestinationComponentCatalog(items);
    } catch (_e) {
      /* keep built-in / HTML catalog */
    }
  }

  function setUsbUi() {
    if (btnUsbOpen) btnUsbOpen.disabled = usbOpened || usbLogRunning;
    if (btnUsbClose) btnUsbClose.disabled = !usbOpened && !usbLogRunning;
    if (usbPortEl) usbPortEl.disabled = usbOpened || usbLogRunning;
    updateUsbLogButtons();
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
      let input;
      if (f.enum_values && f.enum_values.length) {
        input = document.createElement("select");
        input.id = `usb-ctrl-field-${f.name}`;
        f.enum_values.forEach((ev) => {
          const opt = document.createElement("option");
          opt.value = ev;
          opt.textContent = ev;
          input.appendChild(opt);
        });
        if (f.default !== undefined && f.default !== null) {
          input.value = String(f.default);
        }
      } else {
        input = document.createElement("input");
        input.id = `usb-ctrl-field-${f.name}`;
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
      }
      input.dataset.field = f.name;
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

  if (usbDestUnlockEl) {
    usbDestUnlockEl.addEventListener("change", () => {
      setUsbDestinationLocked(!usbDestUnlockEl.checked);
    });
    setUsbDestinationLocked(!usbDestUnlockEl.checked);
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
    stopUsbLogStream();
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
        destination_component: selectedDestinationComponent(),
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
        destination_component: selectedDestinationComponent(),
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

  btnUsbLogStart?.addEventListener("click", () => {
    startUsbLogStream();
  });

  btnUsbLogStop?.addEventListener("click", () => {
    stopUsbLogStream();
  });

  btnUsbLogClear?.addEventListener("click", () => {
    usbLogTerm.clear();
  });

  (async function initUsb() {
    loadBindableCommandsForRecorder();
    loadRecorderCommandsForBar();
    loadUsbDestinationComponents();
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
      usbLogRunning = !!stData.log_running;
      if (stData.port && usbPortEl) usbPortEl.value = stData.port;
    } catch (_e) {
      usbOpened = false;
    }
    setUsbUi();
  })();

  connect();
})();

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

  connect();
})();

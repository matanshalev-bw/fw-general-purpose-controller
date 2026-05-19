#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PYTHON="python3"

venv_ok() {
  [[ -f .venv/bin/activate && -x .venv/bin/python ]]
}

create_venv() {
  if ! python3 -m venv .venv 2>/dev/null; then
    return 1
  fi
  venv_ok
}

if ! venv_ok; then
  if [[ -d .venv ]]; then
    echo "Removing incomplete .venv (install python3-venv for a proper virtualenv)…" >&2
    rm -rf .venv
  fi
  if create_venv; then
    .venv/bin/pip install -q -r requirements.txt
  else
    echo "Using system Python (pip install --user -r requirements.txt)…" >&2
    python3 -m pip install -q --user -r requirements.txt 2>/dev/null \
      || python3 -m pip install -q -r requirements.txt
  fi
elif ! .venv/bin/python -c "import fastapi" 2>/dev/null; then
  .venv/bin/pip install -q -r requirements.txt
fi

if venv_ok; then
  # shellcheck disable=SC1091
  source .venv/bin/activate
  PYTHON="python"
fi

export PYTHONPATH="$SCRIPT_DIR:${PYTHONPATH:-}"

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-8765}"
URL="http://${HOST}:${PORT}/"

if command -v xdg-open >/dev/null 2>&1; then
  (sleep 1 && xdg-open "$URL") &
elif command -v open >/dev/null 2>&1; then
  (sleep 1 && open "$URL") &
fi

exec "$PYTHON" -m gpc_recorder --host "$HOST" --port "$PORT"

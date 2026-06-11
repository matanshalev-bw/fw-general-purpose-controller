"""Run GPC Sequence Recorder: `python -m gpc_recorder`"""

import argparse
import sys

import uvicorn

from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.paths import TOOL_DIR


def _stdin_repl() -> None:
    engine = ReplEngine()
    print("GPC Sequence Recorder (stdin REPL). Type help(), exit to quit.")
    if engine.startup_message:
        print(engine.startup_message)
    while True:
        try:
            line = input(">>> ")
        except (EOFError, KeyboardInterrupt):
            print()
            break
        out, cont = engine.execute(line)
        if out:
            print(out)
        if not cont:
            break


def main() -> None:
    parser = argparse.ArgumentParser(description="GPC Sequence Recorder")
    parser.add_argument("--repl", action="store_true", help="stdin REPL instead of web server")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()

    if args.repl:
        _stdin_repl()
        return

    sys.path.insert(0, str(TOOL_DIR))
    uvicorn.run("gpc_recorder.server:app", host=args.host, port=args.port, reload=False)


if __name__ == "__main__":
    main()

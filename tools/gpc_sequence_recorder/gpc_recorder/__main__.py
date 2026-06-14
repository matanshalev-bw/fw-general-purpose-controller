"""Run GPC Sequence Recorder: `python -m gpc_recorder`"""

import argparse
import errno
import os
import socket
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


def _ensure_port_available(host: str, port: int) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        try:
            sock.bind((host, port))
        except OSError as exc:
            if exc.errno == errno.EADDRINUSE:
                print(
                    f"Port {port} is already in use (another gpc-recorder may be running).\n"
                    f"  Stop it: pkill -f 'gpc_recorder'  or  kill the process on :{port}\n"
                    f"  Or use another port: gpc-recorder --port 8766",
                    file=sys.stderr,
                )
                sys.exit(1)
            raise


def main() -> None:
    parser = argparse.ArgumentParser(description="GPC Sequence Recorder")
    parser.add_argument("--repl", action="store_true", help="stdin REPL instead of web server")
    parser.add_argument("--host", default=os.environ.get("HOST", "127.0.0.1"))
    parser.add_argument(
        "--port",
        type=int,
        default=int(os.environ.get("PORT", "8765")),
    )
    args = parser.parse_args()

    if args.repl:
        _stdin_repl()
        return

    _ensure_port_available(args.host, args.port)

    sys.path.insert(0, str(TOOL_DIR))
    uvicorn.run("gpc_recorder.server:app", host=args.host, port=args.port, reload=False)


if __name__ == "__main__":
    main()

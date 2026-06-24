"""PyInstaller entry point for gpc-recorder-repl.exe."""

import sys

from gpc_recorder.__main__ import main

if __name__ == "__main__":
    if "--repl" not in sys.argv:
        sys.argv.insert(1, "--repl")
    main()

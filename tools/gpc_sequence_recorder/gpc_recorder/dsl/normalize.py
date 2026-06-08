"""Rewrite DSL lines into valid Python before exec."""

import re

# bind_command(trigger=X, Struct(...)) is invalid Python (positional after keyword).
_BIND_COMMAND_FIX = re.compile(
    r"bind_command\s*\(\s*(trigger\s*=\s*[^,]+)\s*,\s*(\w+Command\s*\()",
    re.IGNORECASE,
)

# GUI may emit data="[0, 1, 2]" — unwrap to a real list literal.
_LIST_KW_QUOTED = re.compile(
    r'(?P<kw>data|tx_data)\s*=\s*["\'](?P<vals>\[[^\]]*\])["\']',
    re.IGNORECASE,
)


def normalize_line(line: str) -> str:
    line = _BIND_COMMAND_FIX.sub(
        r"bind_command(\1, command_struct=\2",
        line,
    )
    line = _LIST_KW_QUOTED.sub(lambda m: f"{m.group('kw')}={m.group('vals')}", line)
    return line

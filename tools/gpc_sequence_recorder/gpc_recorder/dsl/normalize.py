"""Rewrite DSL lines into valid Python before exec."""

import re

# begin_binding(trigger=X, Struct(...)) is invalid Python (positional after keyword).
_BEGIN_BINDING_FIX = re.compile(
    r"begin_binding\s*\(\s*(trigger\s*=\s*[^,]+)\s*,\s*(\w+Command\s*\()",
    re.IGNORECASE,
)


def normalize_line(line: str) -> str:
    return _BEGIN_BINDING_FIX.sub(
        r"begin_binding(\1, command_struct=\2",
        line,
    )

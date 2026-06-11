"""Tab-completion candidates for the REPL."""

import re
from typing import Dict, List

from gpc_recorder.schema.cpp_parser import Schema

# Builtin call -> keyword argument names (with trailing '=')
FUNCTION_KEYWORDS: Dict[str, List[str]] = {
    "config": ["name=", "component="],
    "bind_powerup": [],
    "bind_command": ["trigger=", "command_struct="],
    "bind_state": ["state="],
    "clear_state": ["state="],
    "bind_state_tick": ["state="],
    "clear_state_tick": ["state="],
    "gpio_write": ["port=", "pin=", "value="],
    "gpio_read": ["port=", "pin=", "var_index="],
    "adc_read": ["adc_instance=", "channel=", "var_index=", "store_raw="],
    "dac_write": ["dac_instance=", "use_var=", "var_index=", "literal_value="],
    "delay_ms": ["delay_ms="],
    "can_transmit": ["can_bus=", "id=", "dlc=", "data="],
    "pwm_set": ["timer_instance=", "channel=", "use_var=", "var_index=", "literal_duty="],
    "uart_transmit": ["uart_instance=", "length=", "data="],
    "spi_transfer": ["spi_instance=", "tx_len=", "tx_data="],
    "i2c_write": ["i2c_instance=", "device_addr=", "length=", "data="],
    "reload": ["path="],
    "export": ["path="],
}


def _word_at_end(line: str) -> str:
    m = re.search(r"[A-Za-z_][A-Za-z0-9_]*$", line)
    return m.group(0) if m else ""


def _completing_value(line: str, word: str) -> bool:
    """True when cursor is in the value part of keyword=value."""
    if not word:
        return False
    idx = line.rfind(word)
    if idx <= 0:
        return False
    fragment = line[:idx].split("(")[-1].split(",")[-1]
    return "=" in fragment


def _innermost_call(line: str) -> str | None:
    matches = list(re.finditer(r"(\w+)\s*\(", line))
    return matches[-1].group(1) if matches else None


def _innermost_struct_call(line: str) -> str | None:
    """Rightmost *Command( — the struct ctor the cursor is inside."""
    matches = list(re.finditer(r"(\w+Command)\s*\(", line))
    return matches[-1].group(1) if matches else None


def _struct_field_completions(
    schema: Schema, struct_name: str, prefix: str
) -> List[str]:
    if struct_name not in schema.command_structs:
        return []
    fields = [
        f"{field.name}=" for field in schema.command_structs[struct_name].fields
    ]
    if not prefix:
        return fields
    return [f for f in fields if f.startswith(prefix)]


def _enum_values_for_field(schema: Schema, struct_name: str, field_name: str, prefix: str) -> List[str]:
    if struct_name not in schema.command_structs:
        return []
    for field in schema.command_structs[struct_name].fields:
        if field.name != field_name:
            continue
        t = field.cpp_type.split("::")[-1].strip()
        if t in schema.enums:
            return sorted(v for v in schema.enums[t].values if v.startswith(prefix))
    return []


def _kwarg_before_cursor(line: str) -> str | None:
    """Keyword name when cursor is right after '=' or completing its value."""
    m = re.search(r"(\w+)\s*=\s*([A-Za-z_][A-Za-z0-9_]*)?$", line)
    return m.group(1) if m else None


def _bind_command_struct_candidates(
    line: str, schema: Schema, prefix: str
) -> List[str]:
    """Second argument to bind_command: Command struct matching trigger."""
    m = re.search(
        r"bind_command\s*\(\s*(?:trigger\s*=\s*)?(\w+)\s*,\s*(?:command_struct\s*=\s*)?([A-Za-z_][A-Za-z0-9_]*)?$",
        line,
    )
    if not m:
        return []
    trigger = m.group(1)
    struct_prefix = m.group(2) or ""
    mapped = schema.payload_id_to_struct.get(trigger)
    if mapped:
        return [mapped] if mapped.startswith(struct_prefix) else []
    return sorted(
        name
        for name in schema.command_structs
        if name.endswith("Command") and name.startswith(struct_prefix)
    )


def _payload_types(schema: Schema, _namespace_keys: List[str], prefix: str) -> List[str]:
    """PayloadTypeIds that have a matching *Command struct (bindable triggers)."""
    return sorted(
        pid for pid in schema.payload_id_to_struct if pid.startswith(prefix)
    )


def _values_for_kwarg(
    schema: Schema,
    namespace_keys: List[str],
    kw: str,
    prefix: str,
) -> List[str]:
    if kw == "trigger":
        return _payload_types(schema, namespace_keys, prefix)
    return sorted(name for name in namespace_keys if name.startswith(prefix))


def _field_name_before_equals(line: str) -> str | None:
    m = re.search(r"(\w+)\s*=\s*$", line.rstrip())
    return m.group(1) if m else None


def complete_line(schema: Schema, namespace_keys: List[str], line: str) -> List[str]:
    word = _word_at_end(line)
    stripped = line.rstrip()
    inner_struct = _innermost_struct_call(line)

    # Inside *Command(...) — struct fields or enum values (not global payload list)
    if inner_struct:
        if stripped.endswith("="):
            field = _field_name_before_equals(line)
            if field:
                enums = _enum_values_for_field(schema, inner_struct, field, "")
                if enums:
                    return enums
        if _completing_value(line, word):
            kw = _kwarg_before_cursor(line)
            if kw:
                enums = _enum_values_for_field(schema, inner_struct, kw, word)
                if enums:
                    return enums
        if stripped.endswith("(") or word or stripped.endswith(","):
            fields = _struct_field_completions(schema, inner_struct, word)
            if fields:
                return fields

    # Cursor immediately after "keyword=" with no value typed yet
    if stripped.endswith("="):
        m = re.search(r"(\w+)\s*=\s*$", stripped)
        kw = m.group(1) if m else None
        if kw:
            return _values_for_kwarg(schema, namespace_keys, kw, "")
        return []

    if not word and not stripped.endswith(("(", ",")):
        return []

    # After keyword= outside struct: trigger / component / enums
    if _completing_value(line, word):
        kw = _kwarg_before_cursor(line)
        if kw:
            return _values_for_kwarg(schema, namespace_keys, kw, word)
        return sorted(name for name in namespace_keys if name.startswith(word))

    # bind_command( — suggest keyword first (trigger=…)
    if re.search(r"bind_command\s*\(\s*$", line):
        return ["trigger="]

    struct_candidates = _bind_command_struct_candidates(line, schema, word)
    if struct_candidates:
        return struct_candidates

    matches: List[str] = []
    for name in namespace_keys:
        if name.startswith(word):
            matches.append(name)

    inner_call = _innermost_call(line)
    if inner_call and inner_call in FUNCTION_KEYWORDS:
        for kw in FUNCTION_KEYWORDS[inner_call]:
            if kw.startswith(word):
                matches.append(kw)

    return sorted(set(matches))


def longest_common_prefix(strings: List[str]) -> str:
    if not strings:
        return ""
    prefix = strings[0]
    for s in strings[1:]:
        while s and not s.startswith(prefix):
            prefix = prefix[:-1]
            if not prefix:
                return ""
    return prefix

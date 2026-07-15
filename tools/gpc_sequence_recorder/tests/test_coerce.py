"""Byte list coercion tests."""

import pytest

from gpc_recorder.dsl.coerce import (
    coerce_int_byte_list,
    coerce_var_set_value,
    pack_le_bytes_to_uint64,
    quoted_string_bytes,
)
from gpc_recorder.usb_bridge import pack_micro_op_hex


def test_quoted_string_bytes_double_quotes():
    assert quoted_string_bytes('"HELLO"') == [0x48, 0x45, 0x4C, 0x4C, 0x4F]


def test_quoted_string_bytes_single_quotes():
    assert quoted_string_bytes("'HELLO'") == [0x48, 0x45, 0x4C, 0x4C, 0x4F]


def test_coerce_int_byte_list_from_quoted_string():
    assert coerce_int_byte_list('"HELLO"') == [0x48, 0x45, 0x4C, 0x4C, 0x4F]


def test_coerce_int_byte_list_from_single_element_quoted_list():
    assert coerce_int_byte_list(['"HELLO"']) == [0x48, 0x45, 0x4C, 0x4C, 0x4F]


def test_coerce_int_byte_list_from_comma_separated():
    assert coerce_int_byte_list("0x48, 69, 76") == [0x48, 69, 76]


def test_pack_uart_transmit_quoted_string_sets_length():
    payload_hex = pack_micro_op_hex(
        "uart_transmit",
        {
            "uart_instance": 1,
            "length": 0,
            "data": '"HELLO"',
        },
    )
    assert payload_hex == "010548454c4c4f000000"


def test_pack_trigger_safety_and_usb_catalog():
    from gpc_recorder.usb_bridge import micro_ops_catalog, payload_type_id

    assert pack_micro_op_hex("trigger_safety", {"safety_en": 0}) == "00"
    assert pack_micro_op_hex("trigger_safety", {"safety_en": 1}) == "01"
    assert payload_type_id("MICRO_TRIGGER_SAFETY_COMMAND") == 111
    assert any(op["union_member"] == "trigger_safety" for op in micro_ops_catalog())


def test_coerce_int_byte_list_rejects_invalid_string():
    with pytest.raises(ValueError, match="quoted string"):
        coerce_int_byte_list("HELLO")


def test_pack_le_bytes_to_uint64_matches_comm_rx():
    # Same LE pack as GPC storeCommRxBytes / memcpy into uint64_t
    assert pack_le_bytes_to_uint64([0x0A, 0x03, 0x33]) == 0x33030A
    assert pack_le_bytes_to_uint64([10, 3, 51]) == 0x33030A
    assert pack_le_bytes_to_uint64([0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11]) == 0x1122334455667788


def test_coerce_var_set_value_from_array_forms():
    assert coerce_var_set_value([0x0A, 0x03, 0x33]) == 0x33030A
    assert coerce_var_set_value("[0x0A, 0x03, 0x33]") == 0x33030A
    assert coerce_var_set_value("10, 3, 51") == 0x33030A
    assert coerce_var_set_value(3500) == 3500
    assert coerce_var_set_value("0x1234") == 0x1234


def test_pack_var_set_byte_array_via_usb_bridge():
    # var_index=1, reserved=0,0,0, value=0x33030A LE
    payload_hex = pack_micro_op_hex(
        "var_set",
        {"var_index": 1, "value": [0x0A, 0x03, 0x33]},
    )
    assert payload_hex == "010000000a03330000000000"

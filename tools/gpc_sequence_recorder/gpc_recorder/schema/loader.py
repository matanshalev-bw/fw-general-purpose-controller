from pathlib import Path

from gpc_recorder.paths import BLUELINK_MSG, PAYLOAD_STRUCTS, REPO_ROOT
from gpc_recorder.schema.cpp_parser import Schema

_schema: Schema | None = None


def get_schema() -> Schema:
    global _schema
    if _schema is None:
        _schema = Schema()
        _schema.load(
            PAYLOAD_STRUCTS / "PayloadTypes.hpp",
            PAYLOAD_STRUCTS / "CommandsPayloadClasses.hpp",
            PAYLOAD_STRUCTS / "TelemetryPayloadClasses.hpp",
            PAYLOAD_STRUCTS / "PayloadFieldDefinitions.hpp",
            PAYLOAD_STRUCTS / "MicroOpsPayloadClasses.hpp",
            BLUELINK_MSG / "distributed_can_id.hpp",
        )
    return _schema


def reload_schema() -> Schema:
    global _schema
    _schema = None
    return get_schema()

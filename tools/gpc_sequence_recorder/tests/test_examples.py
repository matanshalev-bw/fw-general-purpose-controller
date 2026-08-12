"""Example config list/resolve/load helpers."""

import pytest

from gpc_recorder.graph_api import (
    list_example_configs,
    load_graph_from_config,
    resolve_example_path,
)
from gpc_recorder.paths import EXAMPLES_DIR


_MINIMAL_HPP = """\
#ifndef G474_GPC_CONFIG_MEMORY_HPP_
#define G474_GPC_CONFIG_MEMORY_HPP_

#include "config_memory.hpp"
#include "distributed_can_id.hpp"
#include "PayloadTypes.hpp"

volatile static const FLASH_CONFIG_SECTION ConfigMemory G_CONFIG_READ_ONLY_MEMORY = {
    .config_type = {
        .name = "EXAMPLE_CONFIG",
        .type = ConfigTypeEnum::GPC_CONFIG,
    },
    .bluelink_identity_config = {
        .component_id = bluelink::ComponentId::COMPONENT_ID_THROTTLE_CONTROLLER,
    },
    .sequences_config = {
        .powerup_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .main_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .init_state_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .manual_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .disengagement_state_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .engaged_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .power_up_bit_state_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .operational_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .error_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .emergency_state_tick_sequence = {
            .step_count = 0,
            .steps = {
            },
        },
        .binding_count = 0,
        .bindings = {
        },
        .telemetry_config = {
            .binding_count = 0,
            .bindings = {
            },
        },
    },
};

#endif  // G474_GPC_CONFIG_MEMORY_HPP_
"""


def test_list_example_configs_sorted(tmp_path):
    (tmp_path / "throttle_config.hpp").write_text("// a")
    (tmp_path / "brakes_config.hpp").write_text("// b")
    (tmp_path / "notes.txt").write_text("ignore")
    (tmp_path / "subdir").mkdir()
    assert list_example_configs(tmp_path) == ["brakes_config.hpp", "throttle_config.hpp"]


def test_list_example_configs_missing_dir(tmp_path):
    assert list_example_configs(tmp_path / "missing") == []


@pytest.mark.parametrize(
    "name",
    [
        "../secrets.hpp",
        "..",
        "/",
        "foo/bar.hpp",
        "foo\\bar.hpp",
        "",
        "not_hpp.txt",
    ],
)
def test_resolve_example_path_rejects_traversal(tmp_path, name):
    (tmp_path / "ok_config.hpp").write_text("// ok")
    with pytest.raises(ValueError):
        resolve_example_path(name, tmp_path)


def test_resolve_example_path_missing(tmp_path):
    with pytest.raises(FileNotFoundError):
        resolve_example_path("missing_config.hpp", tmp_path)


def test_resolve_example_path_ok(tmp_path):
    target = tmp_path / "brakes_config.hpp"
    target.write_text("// brakes")
    assert resolve_example_path("brakes_config.hpp", tmp_path) == target.resolve()


def test_load_graph_from_example(tmp_path):
    path = tmp_path / "throttle_config.hpp"
    path.write_text(_MINIMAL_HPP)
    graph = load_graph_from_config(resolve_example_path("throttle_config.hpp", tmp_path))
    assert graph["config"]["name"] == "EXAMPLE_CONFIG"
    assert graph["config"]["component"] == "COMPONENT_ID_THROTTLE_CONTROLLER"


def test_repo_examples_dir_has_expected_files():
    names = list_example_configs(EXAMPLES_DIR)
    assert "brakes_config.hpp" in names
    assert "handbrake_config.hpp" in names
    assert "throttle_config.hpp" in names
    brakes = resolve_example_path("brakes_config.hpp", EXAMPLES_DIR)
    graph = load_graph_from_config(brakes)
    assert graph["config"]["component"] == "COMPONENT_ID_BRAKES_CONTROLLER"
    assert isinstance(graph["containers"], list)

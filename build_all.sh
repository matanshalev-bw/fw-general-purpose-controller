#!/bin/bash

# Local build script for fw-general-purpose-controller
# Builds G474 config firmware (config_g474.bin) via CMake presets (no STM32CubeIDE required).

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

gpc_user_data_dir() {
    if [ -n "${GPC_RECORDER_DATA:-}" ]; then
        echo "$GPC_RECORDER_DATA"
        return 0
    fi
    if [ "$REPO_ROOT" = "/opt/gpc-recorder/repo" ]; then
        echo "${XDG_CACHE_HOME:-$HOME/.cache}/gpc-recorder"
        return 0
    fi
    return 1
}

USER_DATA_DIR="$(gpc_user_data_dir 2>/dev/null || true)"
if [ -n "$USER_DATA_DIR" ]; then
    export GPC_RECORDER_DATA="$USER_DATA_DIR"
    OUT_DIR="${USER_DATA_DIR}/build"
else
    OUT_DIR="${REPO_ROOT}/out"
fi

show_usage() {
    echo "Usage: $0 [TARGET] [BUILD_TYPE]"
    echo ""
    echo "TARGET options:"
    echo "  config-g474 (default) - Build G474 config firmware (config_g474.bin)"
    echo "  clean                   - Remove all build outputs (out directory)"
    echo ""
    echo "BUILD_TYPE options:"
    echo "  both (default)  - Build Debug and Release"
    echo "  debug           - Build Debug only"
    echo "  release         - Build Release only"
    echo ""
    echo "Examples:"
    echo "  $0                      # config-g474 Debug + Release"
    echo "  $0 config-g474 debug    # config Debug only"
    echo "  $0 clean                # remove out/"
    echo ""
    echo "Requires: cmake, make, and arm-none-eabi-gcc (apt `gcc-arm-none-eabi`, or STM32CubeIDE bundled GNU Tools under `/opt/st/stm32cubeide_*`)."
}

clean_all() {
    echo ""
    echo "Cleaning all build outputs..."
    if [ -n "$USER_DATA_DIR" ]; then
        if [ -d "$USER_DATA_DIR" ]; then
            rm -rf "${USER_DATA_DIR}/build"
            echo "  Removed ${USER_DATA_DIR}/build"
        fi
    elif [ -d "$OUT_DIR" ]; then
        rm -rf "$OUT_DIR"
        echo "  Removed $OUT_DIR"
    else
        echo "  No build output directory found"
    fi
    echo ""
}

check_cmake() {
    if ! command -v cmake &> /dev/null; then
        echo "CMake not found (sudo apt install cmake)"
        exit 1
    fi
    echo "CMake: $(cmake --version | head -n1)"
}

check_arm_toolchain() {
    local missing=()
    local found=false

    if command -v arm-none-eabi-gcc &> /dev/null; then
        found=true
        echo "ARM GCC: $(arm-none-eabi-gcc --version | head -n1)"
    fi

    if [ "$found" = false ]; then
        local toolchain_file="${REPO_ROOT}/cmake/toolchain-paths.cmake"
        if [ -f "$toolchain_file" ]; then
            local toolchain_path
            toolchain_path=$(grep -aE '^set\(TOOLCHAIN_PATH' "$toolchain_file" | head -n1 | sed -E 's/.*"(.*)".*/\1/')
            if [ -n "$toolchain_path" ] && [ -x "${toolchain_path}arm-none-eabi-gcc" ]; then
                found=true
                echo "ARM GCC: ${toolchain_path}arm-none-eabi-gcc"
            fi
        fi
    fi

    if [ "$found" = false ]; then
        local cube_gcc
        cube_gcc=$(find /opt/st/stm32cubeide_*/plugins -path '*/tools/bin/arm-none-eabi-gcc' 2>/dev/null | sort -r | head -n1)
        if [ -n "$cube_gcc" ] && [ -x "$cube_gcc" ]; then
            found=true
            echo "ARM GCC (STM32CubeIDE bundle): $cube_gcc"
        fi
    fi

    if [ "$found" = false ]; then
        missing+=("arm-none-eabi-gcc")
    fi

    if [ ${#missing[@]} -gt 0 ]; then
        echo "Missing ARM toolchain: ${missing[*]}"
        echo "  Ubuntu/Debian: sudo apt install gcc-arm-none-eabi"
        echo "  Or install STM32CubeIDE (bundled GNU Tools are auto-detected)"
        exit 1
    fi
}

build_preset() {
    local preset="$1"
    echo "Building $preset..."
    cmake --preset "$preset"
    cmake --build --preset "$preset"
    echo "  $preset built successfully"
}

build_config_user_data() {
    local build_type="$1"
    local build_dir="${USER_DATA_DIR}/build/config-g474/${build_type}"
    local export_dir="${USER_DATA_DIR}/exports"
    echo "Building config-g474 ${build_type} (user data)..."
    mkdir -p "$build_dir" "$export_dir"
    if [ ! -f "${export_dir}/g474_gpc_config_memory.hpp" ]; then
        echo "Missing ${export_dir}/g474_gpc_config_memory.hpp — export from gpc-recorder first." >&2
        exit 1
    fi
    cmake -S "$REPO_ROOT" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/cmake/stm32-toolchain.cmake" \
        -DBUILD_COMPONENT=fw-config-g4 \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DGPC_EXPORT_CONFIG_DIR="$export_dir"
    cmake --build "$build_dir"
    echo "  config-g474 ${build_type} built successfully"
}

build_config_type() {
    local build_type="$1"
    if [ -n "$USER_DATA_DIR" ]; then
        build_config_user_data "$build_type"
    else
        build_preset "config-g474.${build_type}"
    fi
}

TARGET="${1:-config-g474}"
BUILD_TYPE="${2:-both}"

if [ "$TARGET" = "clean" ]; then
    clean_all
    exit 0
fi

if [ "$TARGET" = "-h" ] || [ "$TARGET" = "--help" ]; then
    show_usage
    exit 0
fi

if [ "$TARGET" != "config-g474" ]; then
    echo "Unknown target: $TARGET"
    show_usage
    exit 1
fi

echo "Build target: $TARGET, type: $BUILD_TYPE"
check_cmake
check_arm_toolchain

cd "$REPO_ROOT"

case "$BUILD_TYPE" in
    both)
        build_config_type "Debug"
        build_config_type "Release"
        ;;
    debug)
        build_config_type "Debug"
        ;;
    release)
        build_config_type "Release"
        ;;
    *)
        echo "Unknown build type: $BUILD_TYPE"
        show_usage
        exit 1
        ;;
esac

echo ""
echo "Config binaries:"
if [ -n "$USER_DATA_DIR" ]; then
    for path in \
        "${USER_DATA_DIR}/exports/config_g474.bin" \
        "${USER_DATA_DIR}/build/config-g474/Debug/config_projects/config_g474/config_g474.bin" \
        "${USER_DATA_DIR}/build/config-g474/Release/config_projects/config_g474/config_g474.bin"; do
        if [ -f "$path" ]; then
            echo "  $path ($(wc -c < "$path") bytes)"
        fi
    done
else
    for path in \
        "${OUT_DIR}/build/config-g474/Debug/config_projects/config_g474/config_g474.bin" \
        "${OUT_DIR}/build/config-g474/Release/config_projects/config_g474/config_g474.bin"; do
        if [ -f "$path" ]; then
            echo "  $path ($(wc -c < "$path") bytes)"
        fi
    done
fi
echo ""
echo "All selected builds completed successfully."

# Seed ~/.cache/gpc-recorder/exports/g474_gpc_config_memory.hpp from the packaged default.
# Sourced by gpc-recorder wrappers; also used from deb postinst.

GPC_SEED_DEFAULT_HPP="${GPC_SEED_DEFAULT_HPP:-/opt/gpc-recorder/repo/configs/ConfigsTypes/g474_gpc_config_memory.hpp}"
GPC_SEED_EXPORT_NAME="g474_gpc_config_memory.hpp"

gpc_cache_permission_hint() {
    local path="$1"
    echo "Error: $path is not writable (often created as root during apt install)." >&2
    echo "Fix: sudo chown -R \"\$USER:\$USER\" \"$(dirname "$path")\"" >&2
    exit 1
}

gpc_require_writable_data_dir() {
    local data_dir="$1"
    local exports_dir dest

    if [ -z "$data_dir" ]; then
        return 0
    fi

    if [ -e "$data_dir" ] && [ ! -w "$data_dir" ]; then
        gpc_cache_permission_hint "$data_dir"
    fi

    mkdir -p "$data_dir"

    exports_dir="${data_dir}/exports"
    if [ -e "$exports_dir" ] && [ ! -w "$exports_dir" ]; then
        gpc_cache_permission_hint "$exports_dir"
    fi

    dest="${exports_dir}/${GPC_SEED_EXPORT_NAME}"
    if [ -f "$dest" ] && [ ! -w "$dest" ]; then
        gpc_cache_permission_hint "$dest"
    fi
}

gpc_seed_exports() {
    local data_dir="$1"
    local src="${2:-$GPC_SEED_DEFAULT_HPP}"
    local dest_dir="${data_dir}/exports"
    local dest="${dest_dir}/${GPC_SEED_EXPORT_NAME}"

    if [ -z "$data_dir" ]; then
        return 0
    fi

    gpc_require_writable_data_dir "$data_dir"
    mkdir -p "$dest_dir"
    if [ ! -f "$dest" ] && [ -f "$src" ]; then
        cp -a "$src" "$dest"
    fi
}

gpc_seed_exports_for_user() {
    local username="$1"
    local home uid gid data_dir dest

    if [ -z "$username" ] || [ "$username" = root ]; then
        return 0
    fi

    home="$(getent passwd "$username" 2>/dev/null | cut -d: -f6)"
    if [ -z "$home" ] || [ ! -d "$home" ]; then
        return 0
    fi

    uid="$(id -u "$username")"
    gid="$(id -g "$username")"
    data_dir="${home}/.cache/gpc-recorder"
    dest="${data_dir}/exports/${GPC_SEED_EXPORT_NAME}"

    install -d -o "$uid" -g "$gid" -m 755 "$data_dir"
    install -d -o "$uid" -g "$gid" -m 755 "$data_dir/exports"

    if [ -f "$dest" ]; then
        chown "$uid:$gid" "$dest"
    elif [ -f "$GPC_SEED_DEFAULT_HPP" ]; then
        cp -a "$GPC_SEED_DEFAULT_HPP" "$dest"
        chown "$uid:$gid" "$dest"
    fi
}

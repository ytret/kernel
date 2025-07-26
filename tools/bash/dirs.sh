# Global *_DIR variable definitions.

function declare_dir_vars() {
    local -r this_file="${BASH_SOURCE[0]}"
    local this_dir
    this_dir="$(dirname -- "$this_file")"

    declare -g REPO_DIR
    REPO_DIR="$(readlink --canonicalize -- "$this_dir/../..")"
    readonly REPO_DIR

    declare -gr GDB_DIR="$REPO_DIR/gdb"
    declare -gr ISO_DIR="$REPO_DIR/isodir"
    declare -gr TOOLS_DIR="$REPO_DIR/tools"

    declare -gr BASH_DIR="$TOOLS_DIR/bash"
}

# Prelude file for sourcing all other *.sh files in this directory.

source "$(dirname ${BASH_SOURCE[0]})/dirs.sh"
declare_dir_vars

source "$BASH_DIR/log.sh"

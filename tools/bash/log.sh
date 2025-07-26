# Logging functions.
#
# To disable colored output, set the environment variable NO_COLOR to a
# non-empty string.

if [[ -n "${NO_COLOR:-}" ]]; then
    declare -r COLOR_RED=""
    declare -r COLOR_GREEN=""
    declare -r COLOR_BLUE=""
    declare -r COLOR_NONE=""
else
    declare -r COLOR_RED="\033[0;31m"
    declare -r COLOR_GREEN="\033[0;32m"
    declare -r COLOR_BLUE="\033[0;34m"
    declare -r COLOR_NONE="\033[0m"
fi

declare -r COLOR_INFO="$COLOR_BLUE"
declare -r COLOR_ERR="$COLOR_RED"

function log_info {
    echo -ne "$COLOR_INFO"
    echo "$@"
    echo -ne "$COLOR_NONE"
}

function log_err {
    echo -ne "$COLOR_ERR"
    echo "$@"
    echo -ne "$COLOR_NONE"
} 1>&2

#!/usr/bin/env bash
# vim: set ts=4 sw=4 noet:

declare -r SCRIPT_DIR="$(dirname -- "${BASH_SOURCE[0]}")"
declare -r CONN_TIMEOUT=2

declare -r DEFAULT_BUILD_DIR="$PWD"
declare -r DEFAULT_SOCKET="ktest.sock"

declare OPT_BUILD_DIR="$DEFAULT_BUILD_DIR"
declare OPT_SOCKET="$DEFAULT_SOCKET"

function usage {
	cat <<-EOF
	Usage: ${BASH_SOURCE[0]} [options]

	Options:
	    -b, --build-dir BUILDDIR
	        Use BUILDDIR as the build directory (default: $DEFAULT_BUILD_DIR).
	    -h, --help
	        Print this message and exit.
	    -s, --socket SOCK
	        Bind to a socket named SOCK in the build directory (default: $DEFAULT_SOCKET).
	EOF
}

function run_qemu {
	local -a qemu_args
	qemu_args+=(-chardev file,id=log,path=ktest-serial-log.txt)
	qemu_args+=(-chardev socket,id=ktest,path=ktest.sock,server=on,wait=on)
	qemu_args+=(-serial chardev:log)
	qemu_args+=(-serial chardev:ktest)
	qemu_args+=(-display none)
	export QEMU_SERIAL_STDIO=no
	"$SCRIPT_DIR"/run_qemu.sh "${qemu_args[@]}"
}

function is_qemu_up {
	ps -p "$QEMU_PID" 2>/dev/null
}

function wait_for_socket {
	local -r timeout_at=$(( SECONDS + CONN_TIMEOUT ))
	while (( SECONDS < timeout_at )); do
		if [[ -S "$SOCKET_PATH" ]]; then
			return 0
		fi
	done
	return 1
}

while (( $# > 0 )); do
	case "$1" in
		-b|--build-dir)
			if (( $# < 2 )); then
				echo "Error: $1: expected a build directory" 1>&2
				exit 1
			fi
			OPT_BUILD_DIR="$2"
			shift 2
			;;
		-h|--help)
			usage
			exit 0
			;;
		-s|--socket)
			if (( $# < 2 )); then
				echo "Error: $1: expected a file name" 1>&2
				exit 1
			fi
			OPT_SOCKET="$2"
			shift 2
			;;
        -*)
            echo "Error: unrecognized option '$1'" 1>&2
            exit 1
            ;;
        *)
            echo "Error: unexpected argument '$1'" 1>&2
            exit 1
            ;;
	esac
done

declare -r SOCKET_PATH="$OPT_BUILD_DIR/$OPT_SOCKET"

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM
run_qemu &
declare -r QEMU_PID=$!

if ! wait_for_socket "$SOCKET_PATH"; then
	echo "Error: QEMU did not create a socket at '$SOCKET_PATH'" 1>&2
	exit 1
fi

nc -U "$SOCKET_PATH" || echo "nc failed" 1>&2

wait

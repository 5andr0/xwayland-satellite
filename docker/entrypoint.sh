#!/bin/sh
set -e

DISPLAY_NUM=1
X11_SOCKET="/tmp/.X11-unix/X${DISPLAY_NUM}"
X11_LOCK="/tmp/.X${DISPLAY_NUM}-lock"
MODE="${MODE:-main}"
HOST_X11_DISPLAY="${HOST_X11_DISPLAY:-:0}"
HOST_RUNTIME_DIR="${HOST_RUNTIME_DIR:-/run/host-xdg}"
HOST_X11_SOCKET_DIR="${HOST_X11_SOCKET_DIR:-/tmp/.X11-host-unix}"

cleanup() {
    if [ -n "${APP_PID:-}" ]; then
        kill "$APP_PID" 2>/dev/null || true
    fi
    if [ -n "${SAT_PID:-}" ]; then
        kill "$SAT_PID" 2>/dev/null || true
        wait "$SAT_PID" 2>/dev/null || true
    fi
}

trap cleanup INT TERM HUP

find_host_xauthority() {
    if [ -n "${HOST_XAUTHORITY:-}" ] && [ -f "${HOST_XAUTHORITY}" ]; then
        printf '%s\n' "${HOST_XAUTHORITY}"
        return 0
    fi

    if [ -n "${XAUTHORITY:-}" ] && [ -f "${XAUTHORITY}" ]; then
        printf '%s\n' "${XAUTHORITY}"
        return 0
    fi

    if [ -d "${HOST_RUNTIME_DIR}" ]; then
        set -- "${HOST_RUNTIME_DIR}"/.mutter-Xwaylandauth.*
        if [ -e "$1" ]; then
            printf '%s\n' "$1"
            return 0
        fi

        if [ -f "${HOST_RUNTIME_DIR}/.Xauthority" ]; then
            printf '%s\n' "${HOST_RUNTIME_DIR}/.Xauthority"
            return 0
        fi
    fi

    return 1
}

run_app() {
    "$@" &
    APP_PID=$!

    wait "$APP_PID"
    return $?
}

is_true() {
    case "$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')" in
        ""|0|false|no|off)
            return 1
            ;;
    esac

    return 0
}

prepare_host_x11_socket() {
    display_name="${HOST_X11_DISPLAY#:}"
    display_name="${display_name%%.*}"
    host_socket="${HOST_X11_SOCKET_DIR}/X${display_name}"
    local_socket="/tmp/.X11-unix/X${display_name}"

    mkdir -p /tmp/.X11-unix

    if [ ! -S "${host_socket}" ]; then
        echo "ERROR: host X11 socket ${host_socket} not found" >&2
        return 1
    fi

    rm -f "${local_socket}"
    ln -s "${host_socket}" "${local_socket}"
}

if [ "$#" -eq 0 ]; then
    if is_true "${QT:-0}"; then
        set -- /usr/local/bin/qt-test
    else
        set -- /usr/local/bin/test
    fi
elif [ "$1" = "test" ]; then
    shift
    set -- /usr/local/bin/test "$@"
elif [ "$1" = "qt-test" ]; then
    shift
    set -- /usr/local/bin/qt-test "$@"
fi

SATELLITE_BIN=/usr/local/bin/xwayland-satellite

case "${MODE}" in
    host_xwl)
        ;;
    upstream)
        SATELLITE_BIN="/opt/xwayland-satellite/bin/upstream/xwayland-satellite"
        ;;
    *)
        SATELLITE_BIN=/usr/local/bin/xwayland-satellite
        ;;
esac

if [ "${MODE}" = "host_xwl" ]; then
    if ! prepare_host_x11_socket; then
        exit 1
    fi

    export DISPLAY="${HOST_X11_DISPLAY}"
    unset WAYLAND_DISPLAY
    export XDG_SESSION_TYPE=x11
    export GDK_BACKEND=x11

    if HOST_AUTH_FILE="$(find_host_xauthority)"; then
        export XAUTHORITY="${HOST_AUTH_FILE}"
    else
        echo "WARNING: no host XAUTHORITY file found; X11 auth may fail" >&2
    fi

    run_app "$@"
    exit $?
fi

# A stopped container can retain stale X11 socket/lock files in its writable /tmp layer.
# Remove them before starting a fresh xwayland-satellite instance.
rm -f "$X11_SOCKET" "$X11_LOCK"

"${SATELLITE_BIN}" ":${DISPLAY_NUM}" &
SAT_PID=$!

while [ ! -S "$X11_SOCKET" ]; do
    if ! kill -0 "$SAT_PID" 2>/dev/null; then
        echo "ERROR: xwayland-satellite exited unexpectedly" >&2
        exit 1
    fi
    sleep 0.1
done

export DISPLAY=":${DISPLAY_NUM}"

unset WAYLAND_DISPLAY
export XDG_SESSION_TYPE=x11
export GDK_BACKEND=x11

run_app "$@"
APP_STATUS=$?

kill "$SAT_PID" 2>/dev/null || true
wait "$SAT_PID" 2>/dev/null || true

exit "$APP_STATUS"

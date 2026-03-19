#!/bin/sh
# configure.sh — run on the host to generate .env for docker compose.
# Detects your UID/GID plus the runtime/display paths used by compose.
set -e

HOST_UID=$(id -u)
HOST_GID=$(id -g)
: "${XDG_RUNTIME_DIR:=/run/user/${HOST_UID}}"
: "${WAYLAND_DISPLAY:=wayland-0}"
: "${HOST_X11_DISPLAY:=${DISPLAY:-:0}}"

cat > .env <<EOF
UID=${HOST_UID}
GID=${HOST_GID}
XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR}
WAYLAND_DISPLAY=${WAYLAND_DISPLAY}
HOST_X11_DISPLAY=${HOST_X11_DISPLAY}
EOF

echo "Written .env"

mkdir -p ./config
echo "Created dirs: config"

#!/bin/sh
# configure.sh — run on the host to generate .env for docker compose.
# Detects your UID/GID plus the runtime/display paths used by compose.
set -e

HOST_UID=$(id -u)
HOST_GID=$(id -g)
: "${XDG_RUNTIME_DIR:=/run/user/${HOST_UID}}"
: "${WAYLAND_DISPLAY:=wayland-0}"
: "${HOST_X11_DISPLAY:=${DISPLAY:-:0}}"

read_gsettings_string() {
	key=$1
	if ! command -v gsettings >/dev/null 2>&1; then
		return 0
	fi

	value=$(gsettings get org.gnome.desktop.interface "$key" 2>/dev/null || true)
	if [ -z "$value" ]; then
		return 0
	fi

	printf '%s' "$value" | sed "s/^'//; s/'$//"
}

read_gsettings_uint() {
	key=$1
	if ! command -v gsettings >/dev/null 2>&1; then
		return 0
	fi

	value=$(gsettings get org.gnome.desktop.interface "$key" 2>/dev/null || true)
	if [ -z "$value" ]; then
		return 0
	fi

	printf '%s' "$value" | sed 's/^uint32 //'
}

desktop_contains() {
	needle=$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')
	desktop=$(printf '%s' "${XDG_CURRENT_DESKTOP:-}" | tr '[:upper:]' '[:lower:]')
	case ":${desktop}:" in
		*":${needle}:"*) return 0 ;;
		*) return 1 ;;
	esac
}

read_kde_config() {
	key=$1
	for cmd in kreadconfig6 kreadconfig5 kreadconfig; do
		if command -v "$cmd" >/dev/null 2>&1; then
			value=$("$cmd" --file kcminputrc --group Mouse --key "$key" 2>/dev/null || true)
			if [ -n "$value" ]; then
				printf '%s' "$value"
				return 0
			fi
		fi
	done

	config_file="${XDG_CONFIG_HOME:-${HOME:-}/.config}/kcminputrc"
	if [ ! -f "$config_file" ]; then
		return 0
	fi

	awk -F= -v key="$key" '
		/^\[/ {
			in_mouse = ($0 == "[Mouse]")
			next
		}
		in_mouse && $1 == key {
			value = substr($0, index($0, "=") + 1)
			gsub(/^[ \t]+|[ \t]+$/, "", value)
			print value
			exit
		}
	' "$config_file"
}

read_xfce_config() {
	property=$1
	if command -v xfconf-query >/dev/null 2>&1; then
		xfconf-query -c xsettings -p "$property" 2>/dev/null || true
	fi
}

detect_cursor_theme() {
	if desktop_contains kde; then
		value=$(read_kde_config cursorTheme)
		[ -n "$value" ] || value=$(read_gsettings_string cursor-theme)
		[ -n "$value" ] || value=$(read_xfce_config /Gtk/CursorThemeName)
	elif desktop_contains xfce; then
		value=$(read_xfce_config /Gtk/CursorThemeName)
		[ -n "$value" ] || value=$(read_gsettings_string cursor-theme)
		[ -n "$value" ] || value=$(read_kde_config cursorTheme)
	else
		value=$(read_gsettings_string cursor-theme)
		[ -n "$value" ] || value=$(read_kde_config cursorTheme)
		[ -n "$value" ] || value=$(read_xfce_config /Gtk/CursorThemeName)
	fi
	printf '%s' "$value"
}

detect_cursor_size() {
	if desktop_contains kde; then
		value=$(read_kde_config cursorSize)
		[ -n "$value" ] || value=$(read_gsettings_uint cursor-size)
		[ -n "$value" ] || value=$(read_xfce_config /Gtk/CursorThemeSize)
	elif desktop_contains xfce; then
		value=$(read_xfce_config /Gtk/CursorThemeSize)
		[ -n "$value" ] || value=$(read_gsettings_uint cursor-size)
		[ -n "$value" ] || value=$(read_kde_config cursorSize)
	else
		value=$(read_gsettings_uint cursor-size)
		[ -n "$value" ] || value=$(read_kde_config cursorSize)
		[ -n "$value" ] || value=$(read_xfce_config /Gtk/CursorThemeSize)
	fi
	printf '%s' "$value"
}

: "${XCURSOR_SIZE:=$(detect_cursor_size)}"
: "${XCURSOR_THEME:=$(detect_cursor_theme)}"

cat > .env <<EOF
UID=${HOST_UID}
GID=${HOST_GID}
XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR}
WAYLAND_DISPLAY=${WAYLAND_DISPLAY}
HOST_X11_DISPLAY=${HOST_X11_DISPLAY}
MODE=${MODE:-main}
XCURSOR_SIZE=${XCURSOR_SIZE}
XCURSOR_THEME=${XCURSOR_THEME}
EOF

echo "Written .env"

mkdir -p ./config
echo "Created dirs: config"

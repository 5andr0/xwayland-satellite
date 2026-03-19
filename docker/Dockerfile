# syntax=docker/dockerfile:1.7
# ── Stage 1: shared build environment ────────────────────────────────────────
FROM ubuntu:24.04 AS build-base

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        git \
        g++ \
        pkg-config \
        clang \
        qtbase5-dev \
        qtdeclarative5-dev \
        qtquickcontrols2-5-dev \
        libx11-dev \
        libxrandr-dev \
        libxcb1-dev \
        libxcb-cursor-dev \
        libegl1 \
    && rm -rf /var/lib/apt/lists/*

RUN curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
ENV PATH="/root/.cargo/bin:${PATH}"

# ── Stage 2: build xwayland-satellite only ───────────────────────────────────
FROM build-base AS satellite-builder

COPY . /src/xwayland-satellite
WORKDIR /src/xwayland-satellite
RUN --mount=type=cache,target=/root/.cargo/registry \
    --mount=type=cache,target=/root/.cargo/git \
    --mount=type=cache,target=/tmp/xsat-target \
    set -eux; \
    mkdir -p /tmp/xsat-bin; \
    for branch in main start_pos_fix dragfix noscale giga_patch; do \
        build_dir="/tmp/xsat-build/${branch}"; \
        git clone --branch "${branch}" --single-branch /src/xwayland-satellite "${build_dir}"; \
        CARGO_TARGET_DIR="/tmp/xsat-target/${branch}" cargo build --manifest-path "${build_dir}/Cargo.toml" --release --locked; \
        mkdir -p "/tmp/xsat-bin/${branch}"; \
        cp "/tmp/xsat-target/${branch}/release/xwayland-satellite" "/tmp/xsat-bin/${branch}/xwayland-satellite"; \
    done

# ── Stage 3: build test apps only ────────────────────────────────────────────
FROM build-base AS app-builder

COPY docker/test.c /tmp/test.c
COPY docker/qt-test.cpp /tmp/qt-test.cpp
COPY docker/qt-test.qml /tmp/qt-test.qml

RUN clang -O2 -Wall -Wextra /tmp/test.c -o /tmp/test -lX11 -lXrandr
RUN moc /tmp/qt-test.cpp -o /tmp/qt-test.moc \
    && g++ -O2 -Wall -Wextra -fPIC /tmp/qt-test.cpp -o /tmp/qt-test $(pkg-config --cflags --libs Qt5Widgets Qt5Qml Qt5Quick Qt5QuickControls2)

# ── Stage 2: runtime ─────────────────────────────────────────────────────────
FROM build-base

ARG DEBIAN_FRONTEND=noninteractive
ARG UID=1000
ARG GID=1000
ENV TZ=Etc/UTC \
    RUST_LOG=warn,xwayland_satellite=debug,xwayland_process=error \
    QT_QPA_PLATFORM=xcb \
    QT_QUICK_CONTROLS_STYLE=Basic \
    BRANCH=main \
    QT=0

RUN apt-get update && apt-get install -y --no-install-recommends \
        xwayland \
        libfontconfig1 \
        libfreetype6 \
        libgl1 \
        libqt5core5a \
        libqt5dbus5 \
        libqt5gui5 \
        libqt5network5 \
        libqt5qml5 \
        libqt5qmlmodels5 \
        libqt5quick5 \
        libqt5quickcontrols2-5 \
        libqt5quicktemplates2-5 \
        libqt5widgets5 \
        libx11-6 \
        libx11-xcb1 \
        libxrandr2 \
        libxcb-icccm4 \
        libxcb-cursor0 \
        libxcb-image0 \
        libxcb-keysyms1 \
        libxcb-randr0 \
        libxcb-render-util0 \
        libxcb-xinerama0 \
        libxcb1 \
        libxkbcommon-x11-0 \
        qml-module-qtquick-controls2 \
        qml-module-qtquick-window2 \
        qml-module-qtquick2 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=satellite-builder /tmp/xsat-bin/main/xwayland-satellite /usr/local/bin/xwayland-satellite
COPY --from=satellite-builder /tmp/xsat-bin/start_pos_fix/xwayland-satellite /opt/xwayland-satellite/bin/start_pos_fix/xwayland-satellite
COPY --from=satellite-builder /tmp/xsat-bin/dragfix/xwayland-satellite /opt/xwayland-satellite/bin/dragfix/xwayland-satellite
COPY --from=satellite-builder /tmp/xsat-bin/noscale/xwayland-satellite /opt/xwayland-satellite/bin/noscale/xwayland-satellite
COPY --from=satellite-builder /tmp/xsat-bin/giga_patch/xwayland-satellite /opt/xwayland-satellite/bin/giga_patch/xwayland-satellite
COPY --from=app-builder /tmp/test /usr/local/bin/test
COPY --from=app-builder /tmp/qt-test /usr/local/bin/qt-test
COPY --from=app-builder /tmp/qt-test.qml /usr/local/bin/qt-test.qml

RUN set -eux; \
    if grp="$(getent group "$GID")"; then \
        groupmod -n test "${grp%%:*}"; \
    else \
        groupadd -g "$GID" test; \
    fi; \
    if usr="$(getent passwd "$UID")"; then \
        usermod -l test "${usr%%:*}"; \
        usermod -d /home/test -m -g test test; \
    else \
        useradd -u "$UID" -g test -m -d /home/test -s /bin/sh test; \
    fi; \
    mkdir -p /tmp/.X11-unix; \
    chmod 1777 /tmp/.X11-unix; \
    mkdir -p /home/test/xwayland-satellite; \
    chown -R "${UID}:${GID}" /home/test

COPY --chown=${UID}:${GID} . /home/test/xwayland-satellite

USER test
WORKDIR /home/test/xwayland-satellite

COPY --chmod=755 docker/entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]

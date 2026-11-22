#!/usr/bin/env bash
# The native Linux build's steps (docs/linux-round.md, group D1), one script so
# that the CI job (build.yml, job `linux`), the release (D2) and a run by hand
# do the same thing:
#
#   .github/scripts/linux-build.sh deps        the packages (apt-get; sudo when not root)
#   .github/scripts/linux-build.sh configure   cmake --preset linux-x86_64 [DXVK from $BOF3X_DXVK_LIBDIR]
#   .github/scripts/linux-build.sh build       cmake --build build-linux-x86_64
#   .github/scripts/linux-build.sh check       the ELFs' shape and their libraries
#   .github/scripts/linux-build.sh selftest    the checks that need no game data
#   .github/scripts/linux-build.sh tree        linux_tree.py --check (after configure)
#   .github/scripts/linux-build.sh versions    the tools' versions (configure prints them too)
#
# Run from the repository's root. Reads no game data and writes only under
# build-linux-x86_64/ and $LINUX_CHECK_DIR (default build-linux-x86_64/ci-check).
#
# The preset is the WSL build of waves A and B (cmake/x86_64-linux.cmake,
# cmake/linux_target.cmake): clang, non-PIE, linked at 0x10000000, SDL3 shared,
# DXVK-native's two libraries built by meson at the commit
# cmake/dxvk_headers.cmake pins. The link fails on any undefined name (GNU ld
# does for an executable), so a build that ends is wave B's gate line "no
# undefined name".
set -euo pipefail

BUILD=build-linux-x86_64
CHECK_DIR=${LINUX_CHECK_DIR:-$BUILD/ci-check}

# What the build needs beyond the runner image (Ubuntu2404-Readme: clang 18,
# CMake 3.31, Ninja and pkg-config are there already):
#   meson, glslang-tools   DXVK-native's build and its shaders (cmake/dxvk_native.cmake)
#   ninja-build, pkgconf   named, so that the list also serves a bare 24.04
#   ccache                 the objects between runs (build.yml's cache)
#   SDL3's headers         X11 (with XTest), Wayland and libdecor, ALSA, PulseAudio,
#                          PipeWire, udev, D-Bus, DRM/GBM, EGL, Vulkan: SDL loads each
#                          at run time and compiles its driver only when the headers
#                          are there, so this list decides what a player's SDL can do
PACKAGES=(
  meson glslang-tools ninja-build pkgconf ccache
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev
  libxkbcommon-dev libwayland-dev libdecor-0-dev libegl-dev
  libasound2-dev libpulse-dev libpipewire-0.3-dev
  libudev-dev libdbus-1-dev libdrm-dev libgbm-dev libvulkan-dev
)

as_root() { if [ "$(id -u)" = 0 ]; then "$@"; else sudo "$@"; fi; }

deps() {
  as_root apt-get update -q
  as_root env DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends "${PACKAGES[@]}"
}

versions() {
  command -v clang++ > /dev/null || { echo "::error::clang++ is not on PATH (cmake/x86_64-linux.cmake names clang)"; exit 1; }
  clang++ --version | head -1
  clang++ -v -E -x c++ /dev/null 2>&1 | grep -i 'selected gcc' || true
  cmake --version | head -1
  echo "ninja $(ninja --version)"
  echo "meson $(meson --version)"
  glslang --version 2>&1 | head -1 || true
  python --version
  command -v ccache > /dev/null && ccache --version | head -1 || true
}

configure() {
  versions
  local args=()
  if command -v ccache > /dev/null; then
    args+=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
  fi
  # DXVK-native built by an earlier run of this preset (build.yml caches its
  # install folder): cmake/dxvk_native.cmake links those and builds nothing.
  if [ -n "${BOF3X_DXVK_LIBDIR:-}" ]; then
    args+=("-DBOF3X_DXVK_LIBDIR=$BOF3X_DXVK_LIBDIR")
  fi
  cmake --preset linux-x86_64 "${args[@]}"
}

build() {
  cmake --build "$BUILD"
  command -v ccache > /dev/null && ccache --show-stats | head -12 || true
}

# The files a player's folder takes from the build (call 5 of section 6): the
# two programs and the three libraries beside them, found by RUNPATH $ORIGIN.
check() {
  local fail=0 f
  for f in bof3x bof3x-launcher libSDL3.so.0 libdxvk_d3d11.so.0 libdxvk_dxgi.so.0; do
    if [ ! -f "$BUILD/$f" ]; then echo "::error file=cmake/linux_target.cmake::$BUILD/$f is missing"; fail=1; fi
  done
  [ $fail = 0 ] || exit 1
  for f in bof3x bof3x-launcher; do
    echo "== $BUILD/$f"
    readelf -h "$BUILD/$f" | grep -E '^\s*(Class|Type|Machine):' | tee "$CHECK_DIR.hdr"
    # Non-PIE (stage LA2): the image's code reaches ours by rel32.
    if ! grep -qE 'Type:\s+EXEC' "$CHECK_DIR.hdr"; then
      echo "::error file=cmake/linux_target.cmake::$f is not a non-PIE executable (above)"; fail=1
    fi
    readelf -d "$BUILD/$f" | grep -E '\((NEEDED|RUNPATH|RPATH)\)' | tee "$CHECK_DIR.dyn"
    if ! grep -qF 'Library runpath: [$ORIGIN]' "$CHECK_DIR.dyn"; then
      echo "::error file=cmake/linux_target.cmake::$f has no RUNPATH \$ORIGIN (above)"; fail=1
    fi
    local need=(libSDL3.so.0)
    [ "$f" = bof3x ] && need+=(libdxvk_d3d11.so.0 libdxvk_dxgi.so.0)
    local lib
    for lib in "${need[@]}"; do
      if ! grep -qF "Shared library: [$lib]" "$CHECK_DIR.dyn"; then
        echo "::error file=cmake/linux_target.cmake::$f does not need $lib (above)"; fail=1
      fi
      # Found beside it, not elsewhere on the system.
      if ! ldd "$BUILD/$f" | grep -E "^\s*$lib => $(cd "$BUILD" && pwd)/$lib " > /dev/null; then
        ldd "$BUILD/$f"
        echo "::error file=cmake/linux_target.cmake::$f does not load $lib from its own folder (above)"; fail=1
      fi
    done
    if ldd "$BUILD/$f" | grep 'not found'; then
      echo "::error::$f needs a library that is not found (above)"; fail=1
    fi
  done
  # Linked high (stage LA2): the first segment at 0x10000000, clear of the
  # image's 0x400000..0x987000 and the moved base's 0xA00000..0xF87000.
  local first
  first=$(readelf -lW "$BUILD/bof3x" | awk '$1 == "LOAD" { print $3; exit }')
  echo "bof3x: first LOAD segment at $first"
  if [ "$((first))" -ne $((0x10000000)) ]; then
    echo "::error file=cmake/linux_target.cmake::bof3x's first segment is at $first, not 0x10000000"; fail=1
  fi
  size "$BUILD/bof3x"
  rm -f "$CHECK_DIR.hdr" "$CHECK_DIR.dyn"
  exit $fail
}

# What runs with no game data. The ELF's own self-test (BOF3X_SELFTEST_ONLY)
# does not: it maps Capcom's image from BOF3.exe or a cache (exe_host_posix.cpp,
# step 11), and the launcher refuses a start with neither ("no game data").
selftest() {
  mkdir -p "$CHECK_DIR"
  local out
  out=$(cd "$CHECK_DIR" && pwd)
  # The drawn launcher with no window and no game (src/launcher/ui/main.cpp,
  # --check): every tab and sub-tab opened, a picture of the title page, then
  # Back and Exit, which saves the settings to --save. SDL's offscreen video
  # driver and dummy sound: no display, no device. No --game and no --ini: the
  # defaults, as on a machine with no install.
  rm -f "$out/saved.ini" "$out/title.png"
  SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy "$BUILD/bof3x-launcher" --check --save "$out/saved.ini" \
    --script "shot $out/title.png;open settings;open settings 1;open settings 2;open controls;open controls 0 1;open controls 0 2;open music;open game;open mods;open data;report jukebox;press Back;press Exit"
  test -s "$out/title.png" || { echo "::error file=src/launcher/ui/main.cpp::the launcher's check wrote no picture"; exit 1; }
  test -s "$out/saved.ini" || { echo "::error file=src/launcher/ui/main.cpp::the launcher's check saved no settings"; exit 1; }
  echo "saved.ini: $(wc -l < "$out/saved.ini") lines; title.png: $(wc -c < "$out/title.png") bytes"

  # The keyboard's SDL scancode table against SDL's own Windows table, and the
  # drawn bind capture against the Win32 capture's numbers (tools/key_host):
  # SDL's source as the build fetched it, no SDL library.
  cmake -S tools/key_host -B "$out/key-host" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_COMPILER=clang++ -DSDL_SRC="$(pwd)/$BUILD/_deps/sdl3-src"
  cmake --build "$out/key-host"
  "$out/key-host/key_tests"
  "$out/key-host/capture_tests"
}

# The round's gate line "linux_tree.py --check": every .cpp under src/ but the
# Windows-only and skipped ones compiled -fsyntax-only for x86_64-linux-gnu,
# with no stand-in for a Windows header. Wider than the build, which compiles
# the lists CMake holds: it also sees a file that no list takes. Reads the
# SDL and DXVK headers the configure fetched (build*/_deps).
tree() {
  python tools/linux_probe/linux_tree.py --check --jobs "$(nproc)"
}

case "${1:-}" in
  deps) deps ;;
  versions) versions ;;
  configure) configure ;;
  build) build ;;
  check) check ;;
  selftest) selftest ;;
  tree) tree ;;
  *) echo "usage: $0 deps|versions|configure|build|check|selftest|tree" >&2; exit 2 ;;
esac

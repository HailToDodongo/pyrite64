#!/usr/bin/env bash
#
# Builds a portable Linux AppImage of the Pyrite64 editor.
#
# Usage:
#   scripts/build_appimage.sh            # configure + build (linux-release), then package
#   SKIP_BUILD=1 scripts/build_appimage.sh   # package an already-built ./pyrite64
#
# Output: ./Pyrite64-x86_64.AppImage
#
# Notes:
#  - data/ and n64/ are placed next to the binary so SDL_GetBasePath() finds them.
#  - linuxdeploy bundles libstdc++/libgcc; glibc is NOT bundled, so build on the
#    oldest glibc you want to support (CI uses ubuntu-22.04 => glibc 2.35).
#  - Tool AppImages run without FUSE via APPIMAGE_EXTRACT_AND_RUN.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ARCH="${ARCH:-x86_64}"
APPDIR="$ROOT/build/AppDir"
TOOLS="$ROOT/build/appimage-tools"
export APPIMAGE_EXTRACT_AND_RUN=1

# 1. Build the editor (release) unless asked to skip.
if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  cmake --preset linux-release
  cmake --build --preset linux-release
fi

if [[ ! -f "$ROOT/pyrite64" ]]; then
  echo "error: ./pyrite64 not found (build failed or SKIP_BUILD set without a build)" >&2
  exit 1
fi

# 2. Stage the AppDir. data/ and n64/ live beside the binary in usr/bin.
rm -rf "$APPDIR"
BIN="$APPDIR/usr/bin"
mkdir -p "$BIN/n64"
cp "$ROOT/pyrite64" "$BIN/"
cp -r "$ROOT/data" "$ROOT/LICENSE" "$BIN/"
cp -r "$ROOT/n64/engine" "$BIN/n64/engine"
# bundle all examples, but strip build files that may exists
cp -r "$ROOT/n64/examples" "$BIN/n64/examples"
find "$BIN/n64/examples" -type d \( -name build -o -name engine -o -name filesystem \) -prune -exec rm -rf {} +
find "$BIN/n64/examples" \( -name '*.z64' -o -name '*.pak' \) -delete

# 3. Fetch packaging tools (cached).
mkdir -p "$TOOLS"
fetch() { # url dest
  [[ -f "$2" ]] || { echo "downloading $(basename "$2")"; curl -fL "$1" -o "$2"; chmod +x "$2"; }
}
LD_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
LDP_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-${ARCH}.AppImage"
fetch "$LD_URL"  "$TOOLS/linuxdeploy-${ARCH}.AppImage"
fetch "$LDP_URL" "$TOOLS/linuxdeploy-plugin-appimage-${ARCH}.AppImage"
export PATH="$TOOLS:$PATH"

# 4. Bundle libs + build the AppImage. linuxdeploy preserves the staged data/n64.
rm -f "$ROOT"/Pyrite64-*.AppImage
export OUTPUT="Pyrite64-${ARCH}.AppImage"
"$TOOLS/linuxdeploy-${ARCH}.AppImage" \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/pyrite64" \
  --desktop-file "$ROOT/packaging/pyrite64.desktop" \
  --icon-file "$ROOT/packaging/pyrite64.png" \
  --output appimage

echo "built: $ROOT/$OUTPUT"

#!/bin/bash
set -e

source .venv/bin/activate

# By default we run in FAST mode: the C++ API docs (Doxygen/Breathe/Exhale) are
# skipped, which keeps builds to a few seconds while iterating on the manual/CSS.
# Pass --full (or --api) to build the complete site including the C++ API.
FAST=1
for arg in "$@"; do
  case "$arg" in
    --full|--api) FAST=0 ;;
  esac
done

if [ "$FAST" = "1" ]; then
  export PYRITE_DOCS_FAST=1
  echo "================================================================"
  echo " FAST mode: C++ API docs are SKIPPED. Run with --full for them."
  echo "================================================================"
else
  unset PYRITE_DOCS_FAST
  echo "FULL mode: building everything incl. the C++ API (slower)."
fi

rm -rf _build/html
mkdir -p _build/html

cd _build/html
# pkill -9 -f 8000
sleep 1
python3 -m http.server 8000 &
SERVER_PID=$!
cd ../..

echo "Server started at http://localhost:8000"
echo "Watching for changes..."

make html

while true; do
    # Wait for a change and capture which path triggered it. Exclude the
    # auto-generated C++ API pages: they are (re)written on every full build, so
    # watching them would cause an endless rebuild loop.
    CHANGED=$(inotifywait -r -e modify,create,delete,move \
      --format '%w%f' \
      --exclude 'docs/manual/api/' \
      docs/ _static/ index.rst conf.py Doxyfile _apigen.py ../n64/engine/include/)
    echo "Change detected: $CHANGED"

    if [ "$FAST" = "1" ]; then
        # No C++ API in fast mode -> incremental builds are safe and quick.
        rm -rf _build/html/*
        make html
    else
        case "$CHANGED" in
          *n64/engine/include/* | *Doxyfile | *_apigen.py | *conf.py)
            # A C++ API input changed -> regenerate the API. The regenerated pages
            # don't survive Sphinx incremental builds (stale symbols carry over in
            # the cached doctrees and struct/class members silently get dropped),
            # so clear the doctrees to force a full, correct rebuild.
            echo "  -> C++ API input changed: full rebuild (Doxygen + _apigen)."
            rm -rf _build/html/* _build/doctrees
            make html
            ;;
          *)
            # CSS / manual pages only -> reuse the already-built C++ API and do a
            # quick incremental refresh (skips the slow Doxygen/_apigen step). The
            # html output and doctrees are kept so Sphinx only rebuilds what changed.
            echo "  -> non-API change: quick refresh (reusing C++ API)."
            PYRITE_DOCS_SKIP_DOXYGEN=1 make html
            ;;
        esac
    fi
    echo "Build complete."
done

trap "kill $SERVER_PID" EXIT

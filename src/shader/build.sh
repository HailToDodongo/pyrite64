#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

OUT_DIR="../../data/shader"

mkdir -p "$OUT_DIR"

for shader in *.vert.glsl *.frag.glsl; do
    # Skip if glob didn’t match anything
    [ -e "$shader" ] || continue

    base="${shader%.glsl}"          # n64.vert or n64.frag
    name="${base%.*}"               # n64
    stage="${base##*.}"              # vert or frag

    spv="$OUT_DIR/$base.spv"
    echo "=== Compiling $shader ($stage) ==="

    # GLSL -> SPIR-V
    glslc -fshader-stage="$stage" "$shader" -o "$spv"

done
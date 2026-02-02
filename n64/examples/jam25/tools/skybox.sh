#!/bin/bash

# Check for directory argument
if [ $# -ne 1 ]; then
  echo "Usage: $0 <directory>"
  exit 1
fi

dir="$1"
files=("ny.png" "py.png" "px.png" "nz.png" "nx.png" "pz.png")
sizes=(64)

for file in "${files[@]}"; do
  input="${dir}/${file}"
  base="${file%.*}"
  ext="i8.${file##*.}"

  for size in "${sizes[@]}"; do
    # Rescale and convert to grayscale
    rescaled="${dir}/${base}_${size}.${ext}"
    ffmpeg -y -i "$input" -vf "scale=${size}:${size},format=gray" "$rescaled"

    # Split into 4 tiles (2x2), keep grayscale
    tile_size=$((size / 2))
    ffmpeg -y -i "$rescaled" -filter_complex "crop=${tile_size}:${tile_size}:0:0,format=gray" "${dir}/${base}_${size}_00.${ext}"
    ffmpeg -y -i "$rescaled" -filter_complex "crop=${tile_size}:${tile_size}:${tile_size}:0,format=gray" "${dir}/${base}_${size}_10.${ext}"
    ffmpeg -y -i "$rescaled" -filter_complex "crop=${tile_size}:${tile_size}:0:${tile_size},format=gray" "${dir}/${base}_${size}_01.${ext}"
    ffmpeg -y -i "$rescaled" -filter_complex "crop=${tile_size}:${tile_size}:${tile_size}:${tile_size},format=gray" "${dir}/${base}_${size}_11.${ext}"

    # Delete the rescaled temp file
    rm -f "$rescaled"
  done
done

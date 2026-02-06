set -e

# create temporary directory
BUILD_DIR="build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# make cmake build
cmake -S . -B "$BUILD_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR/build" --config Release --parallel 8

# make dist directory
DIST_DIR="dist"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# copy runtime-data files to dist
mkdir "$DIST_DIR/n64"
cp "pyrite64" "$DIST_DIR/"
cp -r "data" "$DIST_DIR/"
cp -r "docs" "$DIST_DIR/"
cp -r "Readme.md" "$DIST_DIR/"

cp -r "n64/engine" "$DIST_DIR/n64/"
cp -r "n64/examples" "$DIST_DIR/n64/"
cp -r "n64/tiny3d" "$DIST_DIR/n64/"

rm -rf "$DIST_DIR/n64/tiny3d/examples"

# delete all "build" directories in dist
find "$DIST_DIR" -type d -name "build" -exec rm -rf {} +
find "$DIST_DIR" -type f -name "*.z64" -exec rm {} +
find "$DIST_DIR/n64" -type d -name "filesystem" -exec rm -rf {} +

# create .7z archive
ZIP_NAME="pyrite64_$(date +%Y%m%d_%H%M%S).7z"
7z a -t7z "$ZIP_NAME" "$DIST_DIR/*"
7z rn $ZIP_NAME "dist" "pyrite64"

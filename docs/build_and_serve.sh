#!/bin/bash
set -e

source .venv/bin/activate

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
    inotifywait -r -e modify,create,delete,move docs/ index.rst
    echo "Change detected, rebuild..."
    rm -rf _build/html/*
    make html
    echo "Build complete."
done

trap "kill $SERVER_PID" EXIT

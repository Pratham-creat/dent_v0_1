#!/usr/bin/env bash
set -euo pipefail
BUILD_DIR="${BUILD_DIR:-build}"
OUTPUT_DIR="${OUTPUT_DIR:-dist}"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel
cmake --install "$BUILD_DIR" --prefix "$OUTPUT_DIR"
tar -czf dent-linux.tar.gz -C "$OUTPUT_DIR" .
echo "Created dent-linux.tar.gz"

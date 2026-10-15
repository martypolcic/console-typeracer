#!/bin/bash
set -e

if command -v npm >/dev/null 2>&1; then
    (cd gateway && npm install --omit=dev)
fi

mkdir -p build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

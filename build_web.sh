#!/bin/bash

# Build script for osakaOS Web Edition

echo "Building osakaOS for Web..."

# Check if Emscripten is available
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten not found!"
    echo "Please install Emscripten SDK:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

# Build using the Emscripten Makefile
make -f Makefile.emscripten

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo ""
    echo "To run osakaOS:"
    echo "  1. Start a web server: python3 -m http.server 8000"
    echo "  2. Open http://localhost:8000 in your browser"
    echo ""
else
    echo "Build failed!"
    exit 1
fi


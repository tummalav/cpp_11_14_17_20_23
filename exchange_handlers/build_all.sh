#!/bin/bash

# Master Build Script for Exchange Handlers
# Builds all exchange order entry plugins

set -e

echo "======================================================"
echo "Exchange Handlers - Ultra-Low Latency Trading Plugins"
echo "======================================================"
echo ""

# Function to build a plugin
build_plugin() {
    local plugin_dir=$1
    local plugin_name=$2

    echo "Building $plugin_name..."
    echo "------------------------"

    if [ -d "$plugin_dir" ]; then
        cd "$plugin_dir"
        if [ -f "build.sh" ]; then
            ./build.sh
        else
            echo "Warning: No build.sh found in $plugin_dir"
        fi
        cd ..
        echo ""
    else
        echo "Warning: Directory $plugin_dir not found"
        echo ""
    fi
}

# Build all plugins
build_plugin "asx_ouch" "ASX OUCH Protocol Handler"
build_plugin "hkex_ocg" "HKEX OCG-C Protocol Handler"
build_plugin "hkex_omd" "HKEX OMD Market Data Handler"
build_plugin "nasdaq_itch" "NASDAQ ITCH Market Data Handler"

echo "======================================================"
echo "All Exchange Handlers Built Successfully!"
echo "======================================================"
echo ""
echo "Available Plugins:"
echo "  - ASX OUCH:     exchange_handlers/asx_ouch/"
echo "  - HKEX OCG:     exchange_handlers/hkex_ocg/"
echo "  - HKEX OMD:     exchange_handlers/hkex_omd/"
echo "  - NASDAQ ITCH:  exchange_handlers/nasdaq_itch/"
echo ""
echo "Usage:"
echo "  cd exchange_handlers/asx_ouch && ./asx_example"
echo "  cd exchange_handlers/hkex_ocg && ./hkex_example"
echo "  cd exchange_handlers/hkex_omd && ./hkex_omd_example"
echo "  cd exchange_handlers/nasdaq_itch && ./nasdaq_itch_example"
echo ""

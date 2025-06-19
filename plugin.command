#!/bin/bash

# This script automates the process of cloning, building, and installing the AMP plugin for VirtualDJ.
# It's designed to be run on macOS.

# Exit immediately if a command exits with a non-zero status.
set -e

# Define the base directory for VirtualDJ plugins.
PLUGINS_DIR="$HOME/Library/Application Support/VirtualDJ/PluginsMacArm/OnlineSources"
# The name of the directory where we will clone the source code
SRC_DIR="AmpPlugin"
# The final plugin bundle name
PLUGIN_BUNDLE="AMP.bundle"

# Create the plugin directory if it doesn't exist.
echo "==> Ensuring plugin directory exists: $PLUGINS_DIR"
mkdir -p "$PLUGINS_DIR"

# Navigate to the plugin directory.
cd "$PLUGINS_DIR"
echo "==> Changed directory to $PWD"

# Remove any previous build source directory to ensure a fresh clone.
if [ -d "$SRC_DIR" ]; then
    echo "==> Removing existing source directory: $SRC_DIR"
    rm -rf "$SRC_DIR"
fi

# Clone the plugin repository from GitHub.
echo "==> Cloning AMP plugin repository into '$SRC_DIR'..."
git clone https://github.com/Erichy-dev/AmpPlugin.git "$SRC_DIR"

# Navigate into the cloned repository's directory.
cd "$SRC_DIR"
echo "==> Changed directory to $PWD"

# Create a build directory to keep the build files separate from the source.
echo "==> Creating build directory..."
mkdir -p build

# Navigate into the build directory.
cd build
echo "==> Changed directory to $PWD"

# Run CMake to configure the build.
# It looks for the CMakeLists.txt in the parent directory.
echo "==> Running CMake..."
cmake ..

# Run Make to compile the plugin.
# This will create the AMP.bundle.
echo "==> Building plugin with make..."
make

# Remove the existing AMP.bundle if it exists in the destination
if [ -d "../../$PLUGIN_BUNDLE" ]; then
    echo "==> Removing existing $PLUGIN_BUNDLE from destination..."
    rm -rf "../../$PLUGIN_BUNDLE"
fi

# Move the compiled plugin bundle to the main plugins directory.
echo "==> Installing plugin..."
mv "$PLUGIN_BUNDLE" ../../

# Navigate back to the original plugins directory.
cd ../../
echo "==> Changed directory to $PWD"

# Remove the source directory used for building.
echo "==> Cleaning up build files..."
rm -rf "$SRC_DIR"

# Navigate to the home directory.
cd $HOME
echo "==> Changed directory to $PWD"

# Define the document directory.
DOCUMENT_DIR="$HOME/Documents/amp-plugin"
# Remove the document directory if it exists.
if [ -d "$DOCUMENT_DIR" ]; then
    echo "==> Removing existing document directory: $DOCUMENT_DIR"
    rm -rf "$DOCUMENT_DIR"
fi

echo "\n"
echo "************************************************************"
echo "✅  AMP PLUGIN INSTALLED SUCCESSFULLY!  ✅"
echo "   Please close the terminal & restart VirtualDJ"
echo "************************************************************"
echo "\n"

sleep 2  # Give user time to read the success message
osascript -e 'tell application "Terminal" to close front window' &
disown
exit 0
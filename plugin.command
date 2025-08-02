#!/bin/bash

# This script automates the process of cloning, building, and installing the AMP plugin for VirtualDJ.
# It's designed to be run on macOS.

# Exit immediately if a command exits with a non-zero status.
set -e

# Define the base directory for VirtualDJ plugins.
PLUGINS_DIR="$HOME/Library/Application Support/VirtualDJ/PluginsMacArm/OnlineSources"
PLUGINS_DIR2="$HOME/Library/Application Support/VirtualDJ/Plugins64/OnlineSources"
# The name of the directory where we will clone the source code
SRC_DIR="AmpPlugin"
# The final plugin bundle name
PLUGIN_BUNDLE="AMP.bundle"

# Function to check if Homebrew is installed
check_brew() {
    if command -v brew >/dev/null 2>&1; then
        echo "==> Homebrew is already installed"
        return 0
    else
        echo "==> Homebrew is not installed"
        return 1
    fi
}

# Function to install Homebrew
install_brew() {
    echo "==> Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    
    # Add Homebrew to PATH for Apple Silicon Macs
    if [[ $(uname -m) == "arm64" ]]; then
        echo "==> Adding Homebrew to PATH for Apple Silicon Mac"
        eval "$(/opt/homebrew/bin/brew shellenv)"
    else
        echo "==> Adding Homebrew to PATH for Intel Mac"
        eval "$(/usr/local/bin/brew shellenv)"
    fi
}

# Function to check if cmake is installed
check_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        echo "==> cmake is already installed"
        return 0
    else
        echo "==> cmake is not installed"
        return 1
    fi
}

# Function to install cmake via Homebrew
install_cmake() {
    echo "==> Installing cmake via Homebrew..."
    brew install cmake
}

# Function to check if openssl@3 is installed
check_openssl() {
    if brew list openssl@3 >/dev/null 2>&1; then
        echo "==> OpenSSL@3 is already installed"
        return 0
    else
        echo "==> OpenSSL@3 is not installed"
        return 1
    fi
}

# Function to install openssl@3 via Homebrew
install_openssl() {
    echo "==> Installing OpenSSL@3 via Homebrew..."
    brew install openssl@3
}

# Function to check if pkg-config is installed
check_pkgconfig() {
    if command -v pkg-config >/dev/null 2>&1; then
        echo "==> pkg-config is already installed"
        return 0
    else
        echo "==> pkg-config is not installed"
        return 1
    fi
}

# Function to install pkg-config via Homebrew
install_pkgconfig() {
    echo "==> Installing pkg-config via Homebrew..."
    brew install pkg-config
}

# Check and install Homebrew if needed
if ! check_brew; then
    install_brew
fi

# Check and install cmake if needed
if ! check_cmake; then
    install_cmake
fi

# Check and install openssl@3 if needed
if ! check_openssl; then
    install_openssl
fi

# Check and install pkg-config if needed
if ! check_pkgconfig; then
    install_pkgconfig
fi

# Create the plugin directories if they don't exist.
echo "==> Ensuring plugin directories exist: $PLUGINS_DIR"
mkdir -p "$PLUGINS_DIR"
echo "==> Ensuring plugin directory exists: $PLUGINS_DIR2"
mkdir -p "$PLUGINS_DIR2"

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

# Set OpenSSL environment variables for CMake
echo "==> Setting OpenSSL environment variables..."
export OPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3
export OPENSSL_LIBRARIES=/opt/homebrew/opt/openssl@3/lib
export PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@3/lib/pkgconfig:$PKG_CONFIG_PATH"

# Run CMake to configure the build with OpenSSL paths specified.
# It looks for the CMakeLists.txt in the parent directory.
echo "==> Running CMake with OpenSSL configuration..."
cmake -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" -DOPENSSL_LIBRARIES="$OPENSSL_LIBRARIES" ..

# Run Make to compile the plugin.
# This will create the AMP.bundle.
echo "==> Building plugin with make..."
make

# Remove the existing AMP.bundle if it exists in both destinations
if [ -d "../../$PLUGIN_BUNDLE" ]; then
    echo "==> Removing existing $PLUGIN_BUNDLE from PLUGINS_DIR..."
    rm -rf "../../$PLUGIN_BUNDLE"
fi

# Check and remove from PLUGINS_DIR2 as well
PLUGINS_DIR2_BUNDLE="$PLUGINS_DIR2/$PLUGIN_BUNDLE"
if [ -d "$PLUGINS_DIR2_BUNDLE" ]; then
    echo "==> Removing existing $PLUGIN_BUNDLE from PLUGINS_DIR2..."
    rm -rf "$PLUGINS_DIR2_BUNDLE"
fi

# Install the compiled plugin bundle to both plugin directories.
echo "==> Installing plugin to PLUGINS_DIR..."
cp -r "$PLUGIN_BUNDLE" ../../

echo "==> Installing plugin to PLUGINS_DIR2..."
cp -r "$PLUGIN_BUNDLE" "$PLUGINS_DIR2/"

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
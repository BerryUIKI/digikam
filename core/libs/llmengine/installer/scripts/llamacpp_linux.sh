#!/bin/bash
# ==========================================================
#
# This file is a part of digiKam project
# https://www.digikam.org
#
# Date        : 2024-05-19
# Description : Script to download llama.cpp binary for Linux
#
# SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# ==========================================================

set -e

# Function to display usage information
show_usage() {
    echo "Usage: $0 <destination_directory> [version_string] [gpu_type]"
    echo "Downloads the appropriate llama.cpp binary for Linux based on system architecture"
    echo ""
    echo "Arguments:"
    echo "  destination_directory   Base directory (e.g. digikam/llmengine). Binaries are installed into <destination_directory>/llamacpp"
    echo "  version_string          Optional: Version string (e.g., 'b5519'). Defaults to 'b5519' if not specified"
    echo "  gpu_type                Optional: GPU acceleration type. Options: 'cuda', 'vulkan', 'rocm', 'none'"
    echo "                          If not specified, attempts to auto-detect the GPU"
}

# Check if destination directory parameter is provided
if [ $# -lt 1 ] || [ $# -gt 3 ]; then
    echo "Error: Invalid number of arguments."
    show_usage
    exit 1
fi

DEST_DIR="$1"
VERSION="${2:-b5519}"  # Use second parameter or default to latest version (b5519)
GPU_TYPE="${3:-auto}"  # Use third parameter or auto-detect

# Files are installed into a llamacpp subdirectory of the provided destination
INSTALL_DIR="$DEST_DIR/llamacpp"

# Create installation directory if it doesn't exist
if [ ! -d "$INSTALL_DIR" ]; then
    mkdir -p "$INSTALL_DIR"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to create installation directory: $INSTALL_DIR"
        exit 1
    fi
    echo "Created installation directory: $INSTALL_DIR"
fi

# Auto-detect GPU if not specified
if [ "$GPU_TYPE" = "auto" ]; then
    echo "Auto-detecting GPU hardware..."
    
    # Check for NVIDIA GPU with CUDA
    if command -v nvidia-smi &>/dev/null; then
        echo "NVIDIA GPU detected with nvidia-smi present"
        GPU_TYPE="cuda"
    # Check for AMD GPU with ROCm
    elif [ -d "/opt/rocm" ]; then
        echo "ROCm installation found"
        GPU_TYPE="rocm"
    # Check for Vulkan support (works with most GPUs)
    elif command -v vulkaninfo &>/dev/null || [ -f "/usr/lib/libvulkan.so" ] || [ -f "/usr/lib64/libvulkan.so" ]; then
        echo "Vulkan support detected"
        GPU_TYPE="vulkan"
    else
        echo "No supported GPU acceleration detected, using CPU version"
        GPU_TYPE="none"
    fi
fi

echo "Selected GPU type: $GPU_TYPE"

# Determine system architecture
ARCH=$(uname -m)
case $ARCH in
    x86_64)
        ARCH_SUFFIX="x64"
        echo "Detected x86_64 architecture"
        ;;
    aarch64|arm64)
        ARCH_SUFFIX="arm64"
        echo "Detected ARM64 architecture"
        ;;
    armv7*)
        echo "Error: ARM 32-bit architecture is not currently supported by llama.cpp prebuilt binaries"
        exit 1
        ;;
    *)
        echo "Error: Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# GitHub release URL
RELEASE_URL="https://github.com/ggml-org/llama.cpp/releases/download/${VERSION}"

# Determine specific download file based on architecture and GPU support
case $GPU_TYPE in
    cuda)
        # Check CUDA version for appropriate build
        if command -v nvcc &>/dev/null; then
            CUDA_VERSION=$(nvcc --version | grep "release" | sed 's/.*release \([0-9]\+\.[0-9]\+\).*/\1/')
            CUDA_MAJOR=$(echo $CUDA_VERSION | cut -d. -f1)
            CUDA_MINOR=$(echo $CUDA_VERSION | cut -d. -f2)
            
            if [ "$CUDA_MAJOR" -eq "11" ] && [ "$CUDA_MINOR" -ge "7" ]; then
                echo "Using CUDA 11.7+ build"
                DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-cuda11.7-${ARCH_SUFFIX}.tar.gz"
            elif [ "$CUDA_MAJOR" -ge "12" ]; then
                echo "Using CUDA 12+ build"
                DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-cuda12.4-${ARCH_SUFFIX}.tar.gz"
            else
                echo "Warning: CUDA version $CUDA_VERSION may not be supported. Using CPU version."
                DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.tar.gz"
            fi
        else
            echo "Warning: CUDA selected but nvcc not found. Using CPU version."
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.tar.gz"
        fi
        ;;
    vulkan)
        # Check for Vulkan support
        if command -v vulkaninfo &>/dev/null || [ -f "/usr/lib/libvulkan.so" ] || [ -f "/usr/lib64/libvulkan.so" ]; then
            echo "Using Vulkan build"
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-vulkan-${ARCH_SUFFIX}.tar.gz"
        else
            echo "Warning: Vulkan selected but not detected on system. Using CPU version."
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.tar.gz"
        fi
        ;;
    rocm)
        if [ -d "/opt/rocm" ]; then
            echo "Using ROCm build"
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-rocm-${ARCH_SUFFIX}.tar.gz"
        else
            echo "Warning: ROCm selected but not installed. Using CPU version."
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.tar.gz"
        fi
        ;;
    none|*)
        DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.tar.gz"
        ;;
esac

echo "Using llama.cpp version: ${VERSION}"

DOWNLOAD_URL="$RELEASE_URL/$DOWNLOAD_FILE"
TEMP_ARCHIVE="$INSTALL_DIR/llama-cpp-temp.${DOWNLOAD_FILE##*.}"

echo "Downloading llama.cpp binary for Linux ($ARCH)..."
echo "Source: $DOWNLOAD_URL"
echo "Destination: $INSTALL_DIR"

# Download the binary
if command -v curl &>/dev/null; then
    curl -L "$DOWNLOAD_URL" -o "$TEMP_ARCHIVE"
elif command -v wget &>/dev/null; then
    wget "$DOWNLOAD_URL" -O "$TEMP_ARCHIVE"
else
    echo "Error: Neither curl nor wget is available. Please install one of them and try again."
    exit 1
fi

# Check if download was successful
if [ $? -ne 0 ]; then
    echo "Error: Failed to download $DOWNLOAD_FILE"
    exit 1
fi

echo "Download completed. Extracting files..."

# Extract the archive to a temporary extraction directory
TEMP_EXTRACT_DIR="$INSTALL_DIR/temp_extract"
mkdir -p "$TEMP_EXTRACT_DIR"

# Primary format is tar.gz. Keep zip extraction for older release compatibility.
if [[ "$DOWNLOAD_FILE" == *.tar.gz ]]; then
    tar -xzf "$TEMP_ARCHIVE" -C "$TEMP_EXTRACT_DIR"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to extract the tar.gz file."
        rm -rf "$TEMP_EXTRACT_DIR"  # Clean up temp directory
        exit 1
    fi
else
    unzip -o "$TEMP_ARCHIVE" -d "$TEMP_EXTRACT_DIR"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to extract the zip file."
        rm -rf "$TEMP_EXTRACT_DIR"  # Clean up temp directory
        exit 1
    fi
fi

# Remove the temporary archive file
rm "$TEMP_ARCHIVE"

# Move files to installation directory.
# Release archives may use one of two structures:
#   Legacy:  build/bin/<files>
#   Modern:  llama-b<version>/<files>  (single versioned subdirectory, flat layout)
# In both cases we want the files directly in INSTALL_DIR.
if [ -d "$TEMP_EXTRACT_DIR/build/bin" ]; then
    echo "Moving binary files to installation directory..."
    cp -a "$TEMP_EXTRACT_DIR/build/bin/"* "$INSTALL_DIR/"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to copy files from build/bin to installation directory."
        rm -rf "$TEMP_EXTRACT_DIR"
        exit 1
    fi
else
    # Check for a single top-level subdirectory (modern flat layout)
    TOP_DIRS=$(find "$TEMP_EXTRACT_DIR" -mindepth 1 -maxdepth 1 -type d)
    TOP_DIR_COUNT=$(echo "$TOP_DIRS" | grep -c .)
    if [ "$TOP_DIR_COUNT" -eq 1 ]; then
        echo "Moving binary files from versioned subdirectory to installation directory..."
        cp -a "$TOP_DIRS/"* "$INSTALL_DIR/"
        if [ $? -ne 0 ]; then
            echo "Error: Failed to copy files from versioned subdirectory to installation directory."
            rm -rf "$TEMP_EXTRACT_DIR"
            exit 1
        fi
    else
        echo "Warning: Unexpected archive structure. Copying all extracted files."
        cp -a "$TEMP_EXTRACT_DIR/"* "$INSTALL_DIR/"
    fi
fi

# Remove the temporary extraction directory (including any build folder)
rm -rf "$TEMP_EXTRACT_DIR"

# Remove any build directory that might have been copied to the destination
if [ -d "$INSTALL_DIR/build" ]; then
    echo "Removing build directory..."
    rm -rf "$INSTALL_DIR/build"
fi

# Remove any .h files from the destination directory
echo "Removing header files..."
find "$INSTALL_DIR" -name "*.h" -type f -delete

# Make all remaining files executable
echo "Making files executable..."
find "$INSTALL_DIR" -type f -exec chmod +x {} \;

# Check for required libraries
echo "Checking for required libraries..."
MISSING_LIBS=0

check_lib() {
    if ! ldconfig -p | grep -q "$1" 2>/dev/null; then
        echo "Warning: Library $1 not found in system libraries"
        MISSING_LIBS=$((MISSING_LIBS+1))
    fi
}

# Check for common required libraries based on GPU type
case $GPU_TYPE in
    cuda)
        check_lib "libcudart"
        check_lib "libcublas"
        check_lib "libcublasLt"
        ;;
    rocm)
        check_lib "librocblas"
        check_lib "librocsolver"
        ;;
    vulkan)
        check_lib "libvulkan"
        ;;
    *)
        # Check for general libraries needed by the CPU version
        check_lib "libgomp"
        check_lib "libstdc++"
        ;;
esac

if [ $MISSING_LIBS -gt 0 ]; then
    echo "Warning: Some required libraries might be missing. Please check your system."
fi

# Verify installation - check if at least one executable file exists
if [ -f "$INSTALL_DIR/llama-server" ] || [ -f "$INSTALL_DIR/llama-cli" ] || [ -f "$INSTALL_DIR/server" ] || [ -f "$INSTALL_DIR/main" ]; then
    echo "Verifying installation... Found llama.cpp executable files."
else
    echo "Warning: No llama.cpp executable files found in the destination directory."
    echo "This could indicate a problem with the downloaded package or extraction process."
    echo "Please check the contents of: $INSTALL_DIR"
    echo "Available files:"
    ls -la "$INSTALL_DIR"
fi

echo ""
echo "Installation completed successfully."
echo "Llama.cpp binaries are now available in: $INSTALL_DIR"
echo ""
echo "Usage example:"
echo "  $DEST_DIR/main --model [YOUR_MODEL_PATH] --ctx_size 2048 --interactive-first"
echo ""
exit 0

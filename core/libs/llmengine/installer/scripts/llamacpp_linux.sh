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
    echo "  destination_directory   Directory where the binary will be downloaded and extracted"
    echo "  version_string          Optional: Version string (e.g., 'b5415'). Defaults to 'b5415' if not specified"
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
VERSION="${2:-b5415}"  # Use second parameter or default to latest version (b5415)
GPU_TYPE="${3:-auto}"  # Use third parameter or auto-detect

# Create destination directory if it doesn't exist
if [ ! -d "$DEST_DIR" ]; then
    mkdir -p "$DEST_DIR"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to create destination directory: $DEST_DIR"
        exit 1
    fi
    echo "Created destination directory: $DEST_DIR"
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
                DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-cuda11.7-${ARCH_SUFFIX}.zip"
            elif [ "$CUDA_MAJOR" -ge "12" ]; then
                echo "Using CUDA 12+ build"
                DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-cuda12.4-${ARCH_SUFFIX}.zip"
            else
                echo "Warning: CUDA version $CUDA_VERSION may not be supported. Using CPU version."
                DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.zip"
            fi
        else
            echo "Warning: CUDA selected but nvcc not found. Using CPU version."
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.zip"
        fi
        ;;
    vulkan)
        # Check for Vulkan support
        if command -v vulkaninfo &>/dev/null || [ -f "/usr/lib/libvulkan.so" ] || [ -f "/usr/lib64/libvulkan.so" ]; then
            echo "Using Vulkan build"
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-vulkan-${ARCH_SUFFIX}.zip"
        else
            echo "Warning: Vulkan selected but not detected on system. Using CPU version."
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.zip"
        fi
        ;;
    rocm)
        if [ -d "/opt/rocm" ]; then
            echo "Using ROCm build"
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-rocm-${ARCH_SUFFIX}.zip"
        else
            echo "Warning: ROCm selected but not installed. Using CPU version."
            DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.zip"
        fi
        ;;
    none|*)
        DOWNLOAD_FILE="llama-${VERSION}-bin-ubuntu-${ARCH_SUFFIX}.zip"
        ;;
esac

echo "Using llama.cpp version: ${VERSION}"

DOWNLOAD_URL="$RELEASE_URL/$DOWNLOAD_FILE"
TEMP_ZIP="$DEST_DIR/llama-cpp-temp.zip"

echo "Downloading llama.cpp binary for Linux ($ARCH)..."
echo "Source: $DOWNLOAD_URL"
echo "Destination: $DEST_DIR"

# Download the binary
if command -v curl &>/dev/null; then
    curl -L "$DOWNLOAD_URL" -o "$TEMP_ZIP"
elif command -v wget &>/dev/null; then
    wget "$DOWNLOAD_URL" -O "$TEMP_ZIP"
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

# Extract the zip file to a temporary extraction directory
TEMP_EXTRACT_DIR="$DEST_DIR/temp_extract"
mkdir -p "$TEMP_EXTRACT_DIR"
unzip -o "$TEMP_ZIP" -d "$TEMP_EXTRACT_DIR"
if [ $? -ne 0 ]; then
    echo "Error: Failed to extract the zip file."
    rm -rf "$TEMP_EXTRACT_DIR"  # Clean up temp directory
    exit 1
fi

# Remove the temporary zip file
rm "$TEMP_ZIP"

# Move files from build/bin to destination directory
if [ -d "$TEMP_EXTRACT_DIR/build/bin" ]; then
    echo "Moving binary files to destination directory..."
    # Copy all files from build/bin to destination directory
    cp -a "$TEMP_EXTRACT_DIR/build/bin/"* "$DEST_DIR/"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to copy files from build/bin to destination directory."
        rm -rf "$TEMP_EXTRACT_DIR"  # Clean up temp directory
        exit 1
    fi
else
    echo "Warning: build/bin directory not found in the extracted files."
    # Copy all extracted files to the destination as a fallback
    cp -a "$TEMP_EXTRACT_DIR/"* "$DEST_DIR/"
fi

# Remove the temporary extraction directory (including any build folder)
rm -rf "$TEMP_EXTRACT_DIR"

# Remove any build directory that might have been copied to the destination
if [ -d "$DEST_DIR/build" ]; then
    echo "Removing build directory..."
    rm -rf "$DEST_DIR/build"
fi

# Remove any .h files from the destination directory
echo "Removing header files..."
find "$DEST_DIR" -name "*.h" -type f -delete

# Make all remaining files executable
echo "Making files executable..."
find "$DEST_DIR" -type f -exec chmod +x {} \;

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
if [ -f "$DEST_DIR/main" ] || [ -f "$DEST_DIR/server" ]; then
    echo "Verifying installation... Found llama.cpp executable files."
else
    echo "Warning: No llama.cpp executable files found in the destination directory."
    echo "This could indicate a problem with the downloaded package or extraction process."
    echo "Please check the contents of: $DEST_DIR"
    echo "Available files:"
    ls -la "$DEST_DIR"
fi

echo ""
echo "Installation completed successfully."
echo "Llama.cpp binaries are now available in: $DEST_DIR"
echo ""
echo "Usage example:"
echo "  $DEST_DIR/main --model [YOUR_MODEL_PATH] --ctx_size 2048 --interactive-first"
echo ""
exit 0

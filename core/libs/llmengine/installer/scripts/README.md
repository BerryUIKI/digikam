# llama.cpp Installation Scripts

This directory contains platform-specific scripts for downloading and installing llama.cpp binaries.

## Available Scripts

- **llamacpp_linux.sh**: For Linux systems
  - Detects CPU architecture (x64/ARM64) 
  - Auto-detects GPU capabilities (CUDA, ROCm, Vulkan)
  - Downloads appropriate pre-built binaries

- **llamacpp_macos.sh**: For macOS systems
  - Detects CPU architecture (x64/ARM64)
  - Auto-detects GPU capabilities (Metal, Vulkan)
  - Removes quarantine attributes from downloaded binaries

- **llamacpp_win.bat**: For Windows systems
  - Detects CPU architecture (x64/ARM64)
  - Auto-detects GPU capabilities (CUDA, HIP, Vulkan)
  - Uses PowerShell for download and extraction operations

## Usage

These scripts can be called directly from the command line or through the `LlamaCppInstaller` class.

### Command Line Usage

```bash
# Linux and macOS
./llamacpp_linux.sh <destination_directory> [version_string] [gpu_type]
./llamacpp_macos.sh <destination_directory> [version_string] [gpu_type]

# Windows
llamacpp_win.bat <destination_directory> [version_string] [gpu_type]
```

### Parameters

1. `destination_directory`: Directory where the binaries will be installed
2. `version_string`: (Optional) The llama.cpp release version (e.g., b5425)
3. `gpu_type`: (Optional) The GPU acceleration type
   - Linux: auto, cuda, rocm, vulkan, none
   - macOS: auto, metal, vulkan, none
   - Windows: auto, cuda, hip, vulkan, none

## Integration with digiKam

These scripts are used by the `LlamaCppInstaller` class to provide a user-friendly installer
interface for the digiKam application. When the installer is run, it selects the appropriate
script based on the current platform and executes it with the user's chosen parameters.

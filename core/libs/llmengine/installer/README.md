# llama.cpp Installer

This directory contains the implementation of a cross-platform llama.cpp installer for digiKam.

## Components

- **Scripts Directory**: Contains platform-specific shell scripts for downloading and installing llama.cpp binaries:
  - `scripts/llamacpp_linux.sh`: For Linux systems
  - `scripts/llamacpp_macos.sh`: For macOS systems
  - `scripts/llamacpp_win.bat`: For Windows systems

- **C++ Classes**:
  - `LLMEngineInstaller`: Core class that manages the installation process
  - `LLMEngineInstallDialog`: Dialog for the installation UI
  - `LLMEngineInstallAction`: Action class for integration with application menus

## Usage

### Using the Installer Dialog

The easiest way to use the installer is through the `LLMEngineInstallDialog`:

```cpp
#include "llmengineinstalldialog.h"

// Create and show the dialog
Digikam::LLMEngineInstallDialog* dialog = new Digikam::LLMEngineInstallDialog(parentWidget);
dialog->setAttribute(Qt::WA_DeleteOnClose);
dialog->show();
```

### Adding the Installer Action to menus

You can use the `LLMEngineInstallAction` to add an installer option to your application's menus:

```cpp
#include "llmengineinstallaction.h"

// Create the action
Digikam::LLMEngineInstallAction* installAction = new Digikam::LLMEngineInstallAction(this);

// Add it to a menu or toolbar
myMenu->addAction(installAction->action());
```

### Using the Installer Class Directly

For more control, you can use the `LLMEngineInstaller` class directly:

```cpp
#include "llmengineinstaller.h"

// Create the installer
Digikam::LLMEngineInstaller* installer = new Digikam::LLMEngineInstaller(this);

// Connect to signals
connect(installer, &Digikam::LLMEngineInstaller::installationFinished,
        this, &MyClass::handleInstallationFinished);

// Start the installation
QString destDir = "/path/to/install";
installer->start(destDir, "b5425", "auto");  // Version and GPU type are optional
```

## Installation Options

The installer supports the following options:

1. **Destination Directory**: Where to install the llama.cpp binaries
2. **Version**: The version tag from llama.cpp GitHub releases (default: b5425)
3. **GPU Type**: The GPU acceleration to use:
   - `auto`: Auto-detect available GPU support
   - `cuda`: NVIDIA CUDA support
   - `rocm`: AMD ROCm support (Linux)
   - `hip`: AMD HIP support (Windows)
   - `vulkan`: Vulkan support
   - `metal`: Metal support (macOS)
   - `none`: CPU only

## Installation Process

1. The installer detects the platform (Windows, macOS, Linux)
2. It auto-detects GPU capabilities if set to "auto"
3. It finds and downloads the appropriate binary from the llama.cpp GitHub release
4. It extracts the binaries to the specified directory
5. On macOS, it removes quarantine attributes
6. It makes the binaries executable
7. It performs verification to ensure the installation succeeded

If any step fails, a dialog shows the error details.

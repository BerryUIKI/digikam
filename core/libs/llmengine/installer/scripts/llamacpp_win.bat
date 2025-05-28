@echo off
REM ==========================================================
REM
REM This file is a part of digiKam project
REM https://www.digikam.org
REM
REM Date        : 2024-05-19
REM Description : Script to download llama.cpp binary for Windows
REM
REM SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
REM
REM SPDX-License-Identifier: GPL-2.0-or-later
REM
REM ==========================================================

setlocal enabledelayedexpansion

REM Display usage information if no arguments provided
if "%~1"=="" (
    call :show_usage
    exit /b 1
)

REM Parse command line arguments
set "DEST_DIR=%~1"
set "VERSION=%~2"
set "GPU_TYPE=%~3"

REM Set default version if not provided
if "%VERSION%"=="" set "VERSION=b5519"

REM Set default GPU type to auto if not provided
if "%GPU_TYPE%"=="" set "GPU_TYPE=auto"

REM Create destination directory if it doesn't exist
if not exist "%DEST_DIR%" (
    mkdir "%DEST_DIR%" 2>nul
    if errorlevel 1 (
        echo Error: Failed to create destination directory: %DEST_DIR%
        exit /b 1
    )
    echo Created destination directory: %DEST_DIR%
)

REM Auto-detect GPU if not specified
if "%GPU_TYPE%"=="auto" (
    echo Auto-detecting GPU hardware...
    
    REM Check for NVIDIA GPU with CUDA - use nvidiasmi query
    where /Q nvidia-smi >nul 2>&1
    if %errorlevel% equ 0 (
        echo NVIDIA GPU detected with nvidia-smi present
        set "GPU_TYPE=cuda"
        goto :gpu_detected
    )
    
    REM Check for AMD GPU with HIP
    wmic path win32_VideoController get name 2>nul | findstr /i "Radeon\|AMD" >nul
    if %errorlevel% equ 0 (
        REM Check specifically for AMD drivers that support HIP
        where /Q amdgpu-pro-install >nul 2>&1
        if %errorlevel% equ 0 (
            echo AMD GPU with HIP support detected
            set "GPU_TYPE=hip"
        ) else (
            REM Advanced AMD GPU check - look for device IDs that support HIP
            powershell -Command "& {$gpu = Get-WmiObject Win32_VideoController | Where-Object {$_.Name -match 'AMD|Radeon'}; if ($gpu -and $gpu.PNPDeviceID -match 'PCI\\VEN_1002') {exit 0} else {exit 1}}" >nul 2>&1
            if %errorlevel% equ 0 (
                echo AMD GPU detected, checking for compatible drivers
                echo Using Vulkan as a safer option. Specify 'hip' explicitly if you have ROCm support
                set "GPU_TYPE=vulkan"
            ) else (
                echo AMD GPU detected, but requires specific processor verification
                echo Using CPU version as fallback, specify hip if you have appropriate driver support
                set "GPU_TYPE=none"
            )
        )
        goto :gpu_detected
    )
    
    REM Check for Vulkan support - multiple methods
    where /Q vulkaninfo >nul 2>&1
    if %errorlevel% equ 0 (
        echo Vulkan support detected via vulkaninfo tool
        set "GPU_TYPE=vulkan"
        goto :gpu_detected
    )
    
    if exist "%PROGRAMFILES%\Vulkan\*.dll" (
        echo Vulkan SDK installation found
        set "GPU_TYPE=vulkan"
        goto :gpu_detected
    )
    
    if exist "%SystemRoot%\System32\vulkan-1.dll" (
        echo Vulkan runtime detected in system directory
        set "GPU_TYPE=vulkan"
        goto :gpu_detected
    )
    
    echo No supported GPU acceleration detected, using CPU version
    set "GPU_TYPE=none"
)

:gpu_detected
echo Selected GPU type: %GPU_TYPE%

REM Determine system architecture
set "ARCH_SUFFIX=x64"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    echo Detected ARM64 architecture
    set "ARCH_SUFFIX=arm64"
) else (
    echo Detected x64 architecture
)

REM GitHub release URL
set "RELEASE_URL=https://github.com/ggml-org/llama.cpp/releases/download/%VERSION%"

REM Determine specific download file based on architecture and GPU support
if "%GPU_TYPE%"=="cuda" (
    REM Check CUDA version via NVIDIA driver version
    set "CUDA_VERSION="
    
    REM Try multiple methods to detect CUDA version
    where /Q nvcc >nul 2>&1
    if %errorlevel% equ 0 (
        REM Get CUDA version from nvcc if available
        for /f "tokens=* usebackq" %%i in (`nvcc --version ^| findstr "release" 2^>nul`) do (
            for /f "tokens=6" %%v in ("%%i") do set "CUDA_VERSION=%%v"
        )
        echo CUDA version detected from nvcc: !CUDA_VERSION!
    ) else (
        REM Use nvidia-smi to get driver version, then estimate CUDA capability
        for /f "tokens=* usebackq" %%i in (`nvidia-smi --query-gpu^=driver_version --format^=csv,noheader 2^>nul`) do (
            set "DRIVER_VERSION=%%i"
        )
        
        if defined DRIVER_VERSION (
            echo NVIDIA Driver version detected: !DRIVER_VERSION!
            
            for /f "tokens=1 delims=." %%a in ("!DRIVER_VERSION!") do (
                set "DRIVER_MAJOR=%%a"
            )
            
            REM Map driver version to CUDA version
            REM According to NVIDIA compatibility table
            if !DRIVER_MAJOR! GEQ 550 (
                set "CUDA_VERSION=12.5"
            ) else if !DRIVER_MAJOR! GEQ 545 (
                set "CUDA_VERSION=12.4"
            ) else if !DRIVER_MAJOR! GEQ 535 (
                set "CUDA_VERSION=12.3"
            ) else if !DRIVER_MAJOR! GEQ 525 (
                set "CUDA_VERSION=12.1"
            ) else if !DRIVER_MAJOR! GEQ 520 (
                set "CUDA_VERSION=12.0"
            ) else if !DRIVER_MAJOR! GEQ 510 (
                set "CUDA_VERSION=11.8"
            ) else if !DRIVER_MAJOR! GEQ 470 (
                set "CUDA_VERSION=11.7"
            ) else (
                set "CUDA_VERSION=unknown"
            )
            echo Estimated CUDA compatibility: !CUDA_VERSION!
        ) else (
            echo Could not detect NVIDIA driver version
            set "CUDA_VERSION=unknown"
        )
    )
    
    REM Choose appropriate download based on CUDA version
    if "!CUDA_VERSION!"=="unknown" (
        echo Warning: Could not determine CUDA version. Using CPU version as fallback.
        set "DOWNLOAD_FILE=llama-%VERSION%-bin-win-cpu-%ARCH_SUFFIX%.zip"
    ) else (
        for /f "tokens=1 delims=." %%a in ("!CUDA_VERSION!") do (
            set "CUDA_MAJOR=%%a"
        )
        
        if !CUDA_MAJOR! GEQ 12 (
            echo Using CUDA 12.4 build
            set "DOWNLOAD_FILE=llama-%VERSION%-bin-win-cuda12.4-%ARCH_SUFFIX%.zip"
        ) else if !CUDA_MAJOR! GEQ 11 (
            echo Using CUDA 11.7 build
            set "DOWNLOAD_FILE=llama-%VERSION%-bin-win-cuda11.7-%ARCH_SUFFIX%.zip"
        ) else (
            echo Warning: CUDA version !CUDA_VERSION! is older than 11.7.
            echo Using CPU version as fallback.
            set "DOWNLOAD_FILE=llama-%VERSION%-bin-win-cpu-%ARCH_SUFFIX%.zip"
        )
    )
) else if "%GPU_TYPE%"=="hip" (
    echo Using HIP build for AMD GPU
    
    REM Try to detect AMD GPU model for more accurate GFX target
    set "GFX_TARGET=gfx1030"
    
    REM Use PowerShell to attempt to detect AMD GPU model and map to appropriate gfx target
    powershell -Command "& {$gpu = Get-WmiObject Win32_VideoController | Where-Object {$_.Name -match 'AMD|Radeon'}; if ($gpu) {Write-Output $gpu.Name} else {exit 1}}" > "%TEMP%\amd_gpu.txt" 2>nul
    
    if exist "%TEMP%\amd_gpu.txt" (
        set /p AMD_GPU_NAME=<"%TEMP%\amd_gpu.txt"
        echo Detected AMD GPU: !AMD_GPU_NAME!
        
        REM Map common AMD GPUs to their gfx targets
        echo !AMD_GPU_NAME! | findstr /i "RDNA3\|7900\|7800\|7700\|7600" >nul
        if !errorlevel! equ 0 (
            set "GFX_TARGET=gfx1100"
        ) else (
            echo !AMD_GPU_NAME! | findstr /i "RDNA2\|6900\|6800\|6700\|6600" >nul
            if !errorlevel! equ 0 (
                set "GFX_TARGET=gfx1030"
            ) else (
                echo !AMD_GPU_NAME! | findstr /i "RDNA\|5700\|5600\|5500" >nul
                if !errorlevel! equ 0 (
                    set "GFX_TARGET=gfx1010"
                )
            )
        )
        del "%TEMP%\amd_gpu.txt"
    )
    
    set "DOWNLOAD_FILE=llama-%VERSION%-bin-win-hip-%ARCH_SUFFIX%-!GFX_TARGET!.zip"
    echo Using AMD GPU target: !GFX_TARGET!
    echo Warning: If you experience issues, try different HIP build or fall back to Vulkan/CPU
) else if "%GPU_TYPE%"=="vulkan" (
    echo Using Vulkan build
    set "DOWNLOAD_FILE=llama-%VERSION%-bin-win-vulkan-%ARCH_SUFFIX%.zip"
) else (
    echo Using CPU build
    set "DOWNLOAD_FILE=llama-%VERSION%-bin-win-cpu-%ARCH_SUFFIX%.zip"
)

echo Using llama.cpp version: %VERSION%

set "DOWNLOAD_URL=%RELEASE_URL%/%DOWNLOAD_FILE%"
set "TEMP_ZIP=%DEST_DIR%\llama-cpp-temp.zip"

echo Downloading llama.cpp binary for Windows (%ARCH_SUFFIX%)...
echo Source: %DOWNLOAD_URL%
echo Destination: %DEST_DIR%

REM Download the binary using PowerShell with improved error handling
echo Downloading file, please wait...

powershell -Command "& { try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest -Uri '%DOWNLOAD_URL%' -OutFile '%TEMP_ZIP%' -ErrorAction Stop; Write-Host 'Download completed successfully.' } catch { Write-Host 'Error downloading file: ' $_.Exception.Message; exit 1 } }"

if %errorlevel% neq 0 (
    echo Error: Failed to download %DOWNLOAD_FILE%
    echo Please check your internet connection and that the URL exists:
    echo %DOWNLOAD_URL%
    echo.
    echo Try a different version by specifying the version parameter.
    exit /b 1
)

echo Download completed. Extracting files...

REM Extract the zip file to a temporary extraction directory
set "TEMP_EXTRACT_DIR=%DEST_DIR%\temp_extract"
if exist "%TEMP_EXTRACT_DIR%" rmdir /s /q "%TEMP_EXTRACT_DIR%"
mkdir "%TEMP_EXTRACT_DIR%" 2>nul

REM Extract using PowerShell with better error handling
powershell -Command "& { try { Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%TEMP_ZIP%', '%TEMP_EXTRACT_DIR%'); Write-Host 'Extraction completed successfully.' } catch { Write-Host 'Error extracting file: ' $_.Exception.Message; exit 1 } }"

if %errorlevel% neq 0 (
    echo Error: Failed to extract the zip file.
    if exist "%TEMP_EXTRACT_DIR%" rmdir /s /q "%TEMP_EXTRACT_DIR%"
    if exist "%TEMP_ZIP%" del "%TEMP_ZIP%"
    exit /b 1
)

REM Remove the temporary zip file
if exist "%TEMP_ZIP%" del "%TEMP_ZIP%"

REM Move files from build/bin to destination directory
if exist "%TEMP_EXTRACT_DIR%\build\bin" (
    echo Moving binary files to destination directory...
    REM Copy all files from build\bin to destination directory
    xcopy /e /y "%TEMP_EXTRACT_DIR%\build\bin\*" "%DEST_DIR%\" >nul
    if %errorlevel% neq 0 (
        echo Error: Failed to copy files from build\bin to destination directory.
        if exist "%TEMP_EXTRACT_DIR%" rmdir /s /q "%TEMP_EXTRACT_DIR%"
        exit /b 1
    )
) else (
    echo Warning: build\bin directory not found in the extracted files.
    REM Copy all extracted files to the destination as a fallback
    xcopy /e /y "%TEMP_EXTRACT_DIR%\*" "%DEST_DIR%\" >nul
)

REM Remove the temporary extraction directory
if exist "%TEMP_EXTRACT_DIR%" rmdir /s /q "%TEMP_EXTRACT_DIR%"

REM Remove any build directory that might have been copied to the destination
if exist "%DEST_DIR%\build" (
    echo Removing build directory...
    rmdir /s /q "%DEST_DIR%\build"
)

REM Delete any .h files from the destination directory
echo Removing header files...
del /s /q "%DEST_DIR%\*.h" 2>nul

REM Verify installation - check if at least one executable file exists
set FOUND_BINARY=0
if exist "%DEST_DIR%\main.exe" set FOUND_BINARY=1
if exist "%DEST_DIR%\server.exe" set FOUND_BINARY=1

if %FOUND_BINARY% equ 0 (
    echo Warning: No llama.cpp executable files found in the destination directory.
    echo This could indicate a problem with the downloaded package or extraction process.
    echo Please check the contents of: %DEST_DIR%
    echo Available files:
    dir /b "%DEST_DIR%"
) else (
    echo Verifying installation...
    echo Found llama.cpp executable files in: %DEST_DIR%
    
    REM Attempt basic CUDA check if using CUDA
    if "%GPU_TYPE%"=="cuda" (
        where /Q nvidia-smi >nul 2>&1
        if %errorlevel% equ 0 (
            echo.
            echo CUDA information:
            nvidia-smi -L
            echo.
        )
    )
)

echo.
echo Installation completed successfully.
echo Llama.cpp binaries are now available in: %DEST_DIR%
echo.
echo Usage example:
echo   %DEST_DIR%\main.exe --model [YOUR_MODEL_PATH] --ctx_size 2048 --interactive-first
echo.
exit /b 0

:show_usage
echo Usage: %~nx0 ^<destination_directory^> [version_string] [gpu_type]
echo Downloads the appropriate llama.cpp binary for Windows based on system architecture
echo.
echo Arguments:
echo   destination_directory   Directory where the binary will be downloaded and extracted
echo   version_string          Optional: Version string (e.g., 'b5519'). Defaults to 'b5519' if not specified
echo   gpu_type                Optional: GPU acceleration type. Options: 'cuda', 'hip', 'vulkan', 'none'
echo                           If not specified, attempts to auto-detect the GPU
exit /b 0

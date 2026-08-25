#
# SPDX-FileCopyrightText: 2026 by Srirupa Datta, <srirupa dot sps at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Wrapper around the vendored llama.cpp used by the natural-language search
# feature. Keeps all of llama.cpp's build concerns isolated from the rest of
# digiKam: builds llama.cpp as a static library for CPU-only inference,
# disables everything digiKam does not need (demo apps, tools, tests, server,
# GPU backends, network support), and prevents llama.cpp's own install rules
# from leaking into the digiKam install tree.
#
# The vendored source keeps its own CMake build (which correctly handles the
# per-CPU-architecture ggml sources); this wrapper only configures it and
# presents a single static "llama" target for digikamgui to link against.
#

# Library only: no demo apps, tools, tests, server, examples, or network code.
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP      OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON   OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL           OFF CACHE BOOL "" FORCE)

# CPU backend only. GPU backends are left in the vendored source but not built.
set(GGML_CUDA            OFF CACHE BOOL "" FORCE)
set(GGML_METAL           OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN          OFF CACHE BOOL "" FORCE)

# Portable CPU baseline, not the build machine's native CPU. GGML_NATIVE adds
# -march=native, which would crash a distributed AppImage (SIGILL) on an older
# CPU. TODO: revisit for an x86-64-v2 baseline later.
set(GGML_NATIVE          OFF CACHE BOOL "" FORCE)

# Static library linked into digikamgui, not a separate shared object. A static
# library exports no symbols, avoiding the Windows DLL symbol-export problem.
set(BUILD_SHARED_LIBS    OFF CACHE BOOL "" FORCE)

# Suppress llama.cpp's own install rules so it does not install its library,
# headers, pkg-config, or CMake package config into digiKam's prefix.
set(CMAKE_SKIP_INSTALL_RULES ON)

add_subdirectory(llamacpp EXCLUDE_FROM_ALL)

if(NOT APPLE)   # OpenMP under macOS does not support multithreading
    find_package(OpenMP)
endif()

set(CMAKE_SKIP_INSTALL_RULES OFF)

# ggml at -O0 (the Debug default) is unusably slow. Force -O2 on just these
# targets so ggml stays fast while the rest of digiKam remains debuggable.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(_llama_target llama ggml ggml-base ggml-cpu)
        if(TARGET ${_llama_target})
            target_compile_options(${_llama_target} PRIVATE $<$<CONFIG:Debug>:-O2>)
        endif()
    endforeach()
endif()

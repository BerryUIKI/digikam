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

# Don't process automoc on llama.cpp headers

file(GLOB_RECURSE llama_headers ${CMAKE_CURRENT_SOURCE_DIR}/llamacpp/*.h)

foreach(_file ${llama_headers})

    set_property(SOURCE ${_file} PROPERTY SKIP_AUTOMOC ON)

endforeach()

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

# Disable compilation warnings from llama.cpp targets. Just to be clear on the console.

foreach(_llama_target llama ggml ggml-base ggml-cpu)

    if(TARGET ${_llama_target})

        if(MSVC)

            target_compile_options(${_llama_target} PRIVATE /W0)

        else()

            target_compile_options(${_llama_target} PRIVATE -w)

        endif()

    endif()

endforeach()

# Extract config definition to identify the library features

# Variables extraction for llama.cpp

file(READ ${CMAKE_BINARY_DIR}/core/utilities/searchwindow/thirdparty/llamacpp/llama-config.cmake LLAMA_CONFIG_CONTENT)

string(REGEX MATCH "set\\(LLAMA_VERSION[ \t]+([0-9.a-zA-Z]+)\\)" _ ${LLAMA_CONFIG_CONTENT})
string(REPLACE "\"" "" LLAMA_VERSION ${CMAKE_MATCH_1})
set(LLAMA_VERSION ${LLAMA_VERSION} CACHE INTERNAL "Extracted from llama.cpp-config.cmake")

string(REGEX MATCH "set\\(LLAMA_BUILD_COMMIT[ \t]+([0-9a-fA-F]+)\\)" _ ${LLAMA_CONFIG_CONTENT})
string(REPLACE "\"" "" LLAMA_BUILD_COMMIT ${CMAKE_MATCH_1})
set(LLAMA_BUILD_COMMIT ${LLAMA_BUILD_COMMIT} CACHE INTERNAL "Extracted from llama.cpp-config.cmake")

string(REGEX MATCH "set\\(LLAMA_BUILD_NUMBER[ \t]+([0-9]+)\\)" _ ${LLAMA_CONFIG_CONTENT})
string(REPLACE "\"" "" LLAMA_BUILD_NUMBER ${CMAKE_MATCH_1})
set(LLAMA_BUILD_NUMBER ${LLAMA_BUILD_NUMBER} CACHE INTERNAL "Extracted from llama.cpp-config.cmake")

string(REGEX MATCH "set\\(LLAMA_SHARED_LIB[ \t]+(ON|OFF)\\)" _ ${LLAMA_CONFIG_CONTENT})
string(REPLACE "\"" "" LLAMA_SHARED_LIB ${CMAKE_MATCH_1})
set(LLAMA_SHARED_LIB ${LLAMA_SHARED_LIB} CACHE INTERNAL "Extracted from llama.cpp-config.cmake")

message(STATUS "LLAMA.CPP configuration:")
message(STATUS "   LLAMA_VERSION:      ${LLAMA_VERSION}")
message(STATUS "   LLAMA_BUILD_COMMIT: ${LLAMA_BUILD_COMMIT}")
message(STATUS "   LLAMA_BUILD_NUMBER: ${LLAMA_BUILD_NUMBER}")
message(STATUS "   LLAMA_SHARED_LIB:   ${LLAMA_SHARED_LIB}")

# Variables extraction for ggml

file(READ "${CMAKE_BINARY_DIR}/core/utilities/searchwindow/thirdparty/llamacpp/ggml/ggml-config.cmake" GGML_CONFIG_CONTENT)

# Init the list to store the extracted variables

set(GGML_VARIABLES_LIST)

# Divide the contents by lines

string(REPLACE "\n" ";" LINES "${GGML_CONFIG_CONTENT}")

# Parse each line from the file

foreach(LINE IN LISTS LINES)

    # Check if the line corresponds to `set(GGML_* ...)`

    if(LINE MATCHES "^set\\(GGML_[A-Za-z0-9_]+ [^)]+\\)$")

        # Variable name + value extraction

        string(REGEX MATCH "set\\(([A-Za-z0-9_]+) ([^)]+)\\)" MATCH_RESULT "${LINE}")
        string(REGEX REPLACE "set\\(([A-Za-z0-9_]+) ([^)]+)\\)" "\\1" VAR_NAME "${LINE}")
        string(REGEX REPLACE "set\\(([A-Za-z0-9_]+) ([^)]+)\\)" "\\2" VAR_VALUE "${LINE}")

        # Remove quotes from the value

        string(REPLACE "\"" "" VAR_VALUE "${VAR_VALUE}")

        # Append the variable to the list

        list(APPEND GGML_VARIABLES_LIST "${VAR_NAME}")

        set(${VAR_NAME} "${VAR_VALUE}" CACHE INTERNAL "Extracted from ggml-config.cmake")

    endif()

endforeach()

# Show all extracted variables

message(STATUS "GGML configuration:")

foreach(VAR IN LISTS GGML_VARIABLES_LIST)

    message(STATUS "   ${VAR} = ${${VAR}}")

endforeach()

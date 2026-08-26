#
# SPDX-FileCopyrightText: 2026 by Srirupa Datta, <srirupa dot sps at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

if(ENABLE_NLSEARCH_LLAMACPP)

    message(STATUS "Check for the NL Search requirements:")

    if(ENABLE_NLSEARCH_SYSTEM_LLAMACPP)

        message(STATUS "Check for the availability of llama.cpp and ggml libraries from the system...")

        find_package(llama CONFIG)
        find_package(ggml CONFIG)

        if(llama_FOUND AND ggml_FOUND)

            set(HAVE_LLAMACPP TRUE)
            set(HAVE_LLAMACPP_SYSTEM TRUE)

            message(STATUS "NL search results: system llama.cpp backend enabled")
            message(STATUS "llama.cpp version: ${LLAMA_VERSION}")
            message(STATUS "ggml version     : ${GGML_VERSION}")

            # Initialiser les listes pour stocker les variables LLAMA_* et GGML_*

            set(LLAMA_VARIABLES_LIST)
            set(GGML_VARIABLES_LIST)

            # Récupérer toutes les variables CMake

            get_cmake_property(_all_vars VARIABLES)

            # Parcourir toutes les variables et filtrer celles qui commencent par LLAMA_ ou GGML_

            foreach(_var IN LISTS _all_vars)

                if(_var MATCHES "^LLAMA_")

                    list(APPEND LLAMA_VARIABLES_LIST "${_var}")

                elseif(_var MATCHES "^GGML_")

                    list(APPEND GGML_VARIABLES_LIST "${_var}")

                endif()

            endforeach()

            # Show all extracted variables

            message(STATUS "LLAMA.CPP configuration:")

            foreach(VAR IN LISTS LLAMA_VARIABLES_LIST)

                message(STATUS "   ${VAR} = ${${VAR}}")

            endforeach()

            message(STATUS "GGML configuration:")

            foreach(VAR IN LISTS GGML_VARIABLES_LIST)

                message(STATUS "   ${VAR} = ${${VAR}}")

            endforeach()

            # Flush the configuration to the template header

            set(LLAMA_CONFIG_ENTRIES "")
            set(GGML_CONFIG_ENTRIES "")

            # Fill LLAMA_CONFIG_ENTRIES

            foreach(_var IN LISTS LLAMA_VARIABLES_LIST)

                # escape '"' in the values

                string(REPLACE "\"" "" _clean_value "${${_var}}")

                # Append a comma and a cariage return before each entry

                if(LLAMA_CONFIG_ENTRIES STREQUAL "")

                    set(LLAMA_CONFIG_ENTRIES "    { QStringLiteral(\"${_var}\"), QStringLiteral(\"${_clean_value}\") }")

                else()

                    set(LLAMA_CONFIG_ENTRIES "${LLAMA_CONFIG_ENTRIES},\n    { QStringLiteral(\"${_var}\"), QStringLiteral(\"${_clean_value}\") }")

                endif()

            endforeach()

            # Fill GGML_CONFIG_ENTRIES

            foreach(_var IN LISTS GGML_VARIABLES_LIST)

                # escape '"' in the values

                string(REPLACE "\"" "" _clean_value "${${_var}}")

                # Append a comma and a cariage return before each entry

                if(GGML_CONFIG_ENTRIES STREQUAL "")

                    set(GGML_CONFIG_ENTRIES "    { QStringLiteral(\"${_var}\"), QStringLiteral(\"${_clean_value}\") }")

                else()

                    set(GGML_CONFIG_ENTRIES "${GGML_CONFIG_ENTRIES},\n    { QStringLiteral(\"${_var}\"), QStringLiteral(\"${_clean_value}\") }")

                endif()

            endforeach()

            # Configure the header

            configure_file(
                "${CMAKE_SOURCE_DIR}/core/cmake/templates/llamaggmlconfig.h.cmake.in"
                "${CMAKE_BINARY_DIR}/core/app/utils/digikam_llamaggmlconfig.h"
            )

        else()

            message(WARNING "NL search results: system llama.cpp requested but not found; natural-language search disabled")

        endif()

    else()

        set(HAVE_LLAMACPP TRUE)
        message(STATUS "NL search results: bundled llama.cpp backend enabled")

    endif()

endif()

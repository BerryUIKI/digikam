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

        else()

            message(WARNING "NL search results: system llama.cpp requested but not found; natural-language search disabled")

        endif()

    else()

        set(HAVE_LLAMACPP TRUE)
        message(STATUS "NL search results: bundled llama.cpp backend enabled")

    endif()

endif()

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

        else()

            message(WARNING "NL search results: system llama.cpp requested but not found; natural-language search disabled")

        endif()

    else()

        set(HAVE_LLAMACPP TRUE)
        message(STATUS "NL search results: bundled llama.cpp backend enabled")

    endif()

endif()

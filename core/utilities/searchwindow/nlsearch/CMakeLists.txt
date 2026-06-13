# ============================================================
#
# This file is a part of digiKam project
# https://www.digikam.org
#
# SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
# SPDX-License-Identifier: BSD-3-Clause
#
# ============================================================

set(libnlsearch_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchlanguagebackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchmockbackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchpromptbuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchintentparser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchcapabilitydictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchintentresolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchqueryengine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchquerycache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchnlmodelmanager.cpp
)

# llama.cpp backend is optional at build time.

option(ENABLE_NLSEARCH_LLAMACPP "Build llama.cpp backend for natural-language search" OFF)

if(ENABLE_NLSEARCH_LLAMACPP)

    find_package(Llama QUIET)

    if(Llama_FOUND)

        set(HAVE_LLAMACPP TRUE)

        list(APPEND libnlsearch_SRCS
             ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchllamabackend.cpp
        )

        message(STATUS "NL search: llama.cpp backend enabled")

    else()

        message(STATUS "NL search: llama.cpp not found, building with mock backend only")

    endif()

endif()

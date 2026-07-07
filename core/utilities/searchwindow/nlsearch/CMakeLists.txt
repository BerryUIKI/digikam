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

if(HAVE_LLAMACPP)

    list(APPEND libnlsearch_SRCS
         ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/searchllamabackend.cpp
    )

endif()

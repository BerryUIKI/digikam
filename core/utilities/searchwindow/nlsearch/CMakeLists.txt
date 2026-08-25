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

# Object library: NL-search core compiled once, shared by digikamgui and the unit test.

add_library(core_nlsearch_obj OBJECT ${libnlsearch_SRCS})

target_compile_definitions(core_nlsearch_obj
                           PRIVATE
                           digikamgui_EXPORTS
)

target_include_directories(core_nlsearch_obj PRIVATE
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Core,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Gui,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:KF${QT_VERSION_MAJOR}::I18n,INTERFACE_INCLUDE_DIRECTORIES>
    ${CMAKE_CURRENT_SOURCE_DIR}/nlsearch
)

if(HAVE_LLAMACPP)

   target_compile_definitions(core_nlsearch_obj PRIVATE HAVE_LLAMACPP)

   if(HAVE_LLAMACPP_SYSTEM)

      target_link_libraries(core_nlsearch_obj PRIVATE llama ggml)

   else()

      target_include_directories(core_nlsearch_obj PRIVATE
         ${CMAKE_SOURCE_DIR}/core/utilities/searchwindow/thirdparty/llama.cpp/include
         ${CMAKE_SOURCE_DIR}/core/utilities/searchwindow/thirdparty/llama.cpp/ggml/include)

   endif()

endif()

# Static library wrapping the object files, for linking the unit test without recompiling.

add_library(nlsearch STATIC $<TARGET_OBJECTS:core_nlsearch_obj>)

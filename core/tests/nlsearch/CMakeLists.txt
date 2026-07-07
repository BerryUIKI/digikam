#
# SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

include_directories(
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Test,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Gui,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Core,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Widgets,INTERFACE_INCLUDE_DIRECTORIES>

    $<TARGET_PROPERTY:KF${QT_VERSION_MAJOR}::I18n,INTERFACE_INCLUDE_DIRECTORIES>

    ${CMAKE_SOURCE_DIR}/core/utilities/searchwindow/nlsearch
)

set(NLSEARCH_DIR ${CMAKE_SOURCE_DIR}/core/utilities/searchwindow/nlsearch)

set(nlsearchpipeline_utest_SRCS
    nlsearchpipeline_utest.cpp

    ${NLSEARCH_DIR}/searchqueryengine.cpp
    ${NLSEARCH_DIR}/searchlanguagebackend.cpp
    ${NLSEARCH_DIR}/searchmockbackend.cpp
    ${NLSEARCH_DIR}/searchpromptbuilder.cpp
    ${NLSEARCH_DIR}/searchintentparser.cpp
    ${NLSEARCH_DIR}/searchcapabilitydictionary.cpp
    ${NLSEARCH_DIR}/searchintentresolver.cpp
    ${NLSEARCH_DIR}/searchquerycache.cpp
    ${NLSEARCH_DIR}/searchnlmodelmanager.cpp
)

add_executable(nlsearchpipeline_utest ${nlsearchpipeline_utest_SRCS})

target_link_libraries(nlsearchpipeline_utest

                      digikamcore
                      digikamdatabase

                      Qt${QT_VERSION_MAJOR}::Core
                      Qt${QT_VERSION_MAJOR}::Gui
                      Qt${QT_VERSION_MAJOR}::Widgets
                      Qt${QT_VERSION_MAJOR}::Test

                      KF${QT_VERSION_MAJOR}::I18n

                      ${OpenCV_LIBRARIES}
)

add_test(NAME nlsearchpipeline_utest COMMAND nlsearchpipeline_utest)

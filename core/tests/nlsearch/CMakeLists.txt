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
)

add_executable(nlsearchpipeline_utest ${nlsearchpipeline_utest_SRCS})

if(HAVE_LLAMACPP AND TARGET llama)

   target_compile_definitions(nlsearchpipeline_utest PRIVATE HAVE_LLAMACPP)
   target_include_directories(nlsearchpipeline_utest PRIVATE
       ${CMAKE_SOURCE_DIR}/core/utilities/searchwindow/thirdparty/llama.cpp/include)

endif()

target_link_libraries(nlsearchpipeline_utest

                     nlsearch

                     digikamcore
                     digikamdatabase

                     Qt${QT_VERSION_MAJOR}::Core
                     Qt${QT_VERSION_MAJOR}::Gui
                     Qt${QT_VERSION_MAJOR}::Widgets
                     Qt${QT_VERSION_MAJOR}::Test

                     KF${QT_VERSION_MAJOR}::I18n

                     ${OpenCV_LIBRARIES}
)

if(HAVE_LLAMACPP AND TARGET llama)
   target_link_libraries(nlsearchpipeline_utest llama)
endif()

add_test(NAME nlsearchpipeline_utest COMMAND nlsearchpipeline_utest)

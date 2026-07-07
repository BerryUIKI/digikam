#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
# SPDX-FileCopyrightText: 2015      by Veaceslav Munteanu, <veaceslav dot munteanu90 at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

if(HAVE_LLAMACPP)

    # Bundled llama.cpp (pinned git submodule) - library only:
    # CPU backend, no demo apps, tools, tests, or network support.
    set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_APP      OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_COMMON   OFF CACHE BOOL "" FORCE)
    set(LLAMA_CURL           OFF CACHE BOOL "" FORCE)
    set(GGML_CUDA            OFF CACHE BOOL "" FORCE)
    set(GGML_METAL           OFF CACHE BOOL "" FORCE)
    set(GGML_VULKAN          OFF CACHE BOOL "" FORCE)

    add_subdirectory(thirdparty/llama.cpp)

    # ggml/llama are slow without optimization, so forcing it
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

        foreach(_llama_target llama ggml ggml-base ggml-cpu)

            if(TARGET ${_llama_target})
                target_compile_options(${_llama_target} PRIVATE $<$<CONFIG:Debug>:-O2>)
            endif()

        endforeach()

    endif()

endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/CMakeLists.txt) 

set(libsearchwindow_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/searchwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchgroup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchgroup_p.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchgroup_label.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfieldgroup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_album.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_colordepth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_keyword.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_pageorientation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_rangeint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_checkbox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_combobox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_labels.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_rangedate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_recentmodified.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_rating.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_choice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_createfield.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_monthday.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_rangedouble.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfields_text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchutilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ratingsearchutilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/choicesearchutilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchfolderview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchtabheader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchmodificationhelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editablesearchtreeview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/visibilitycontroller.cpp
    ${libnlsearch_SRCS}
)

include_directories($<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Gui,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Sql,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Core,INTERFACE_INCLUDE_DIRECTORIES>

                    $<TARGET_PROPERTY:KF${QT_VERSION_MAJOR}::I18n,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF${QT_VERSION_MAJOR}::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF${QT_VERSION_MAJOR}::XmlGui,INTERFACE_INCLUDE_DIRECTORIES>
)

if(ENABLE_DBUS)
    include_directories($<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::DBus,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

# Used by digikamgui
add_library(gui_searchwindow_obj OBJECT ${libsearchwindow_SRCS})

target_compile_definitions(gui_searchwindow_obj
                           PRIVATE
                           digikamgui_EXPORTS
)

if(HAVE_LLAMACPP)

    target_include_directories(gui_searchwindow_obj
                               PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/llama.cpp/include
                               ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/llama.cpp/ggml/include
    )

endif()

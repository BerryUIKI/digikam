#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
# SPDX-FileCopyrightText: 2015      by Veaceslav Munteanu, <veaceslav dot munteanu90 at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

if(HAVE_LLAMACPP AND NOT HAVE_LLAMACPP_SYSTEM)

    # All vendored llama.cpp build concerns are handled in the wrapper.

    add_subdirectory(thirdparty)

endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/nlsearch/CMakeLists.txt)

set(libsearchwindow_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/view/searchwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/searchwindow_slots.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/searchwindow_nlsearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/searchview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/searchfolderview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/searchtabheader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/searchtabheader_p.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/searchtabheader_nlsearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/searchtabheader_slots.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/utils/searchutilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/searchmodificationhelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ratingsearchutilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/choicesearchutilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/editablesearchtreeview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/visibilitycontroller.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/group/searchgroup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/group/searchgroup_p.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/group/searchgroup_label.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfieldgroup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfieldgrouplabel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_album.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_colordepth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_keyword.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_pageorientation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_rangeint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_checkbox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_combobox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_labels.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_rangedate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_recentmodified.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_rating.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_choice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_createfield.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_monthday.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_rangedouble.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/searchfields_text.cpp
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

   if(HAVE_LLAMACPP_SYSTEM)

       target_link_libraries(gui_searchwindow_obj PRIVATE llama ggml)

   else()

       target_include_directories(gui_searchwindow_obj
                                  PRIVATE
                                  ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/llamacpp/include
                                  ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/llamacpp/ggml/include
       )

   endif()

endif()

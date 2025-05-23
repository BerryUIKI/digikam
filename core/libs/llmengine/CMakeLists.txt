#
# SPDX-FileCopyrightText: 2025 by Gilles Caulier, <caulier dot gilles at gmail dot com>
# SPDX-FileCopyrightText: 2025 by Michael Miller, <michael underscore miller at msn dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

kde_enable_exceptions()

include_directories(
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Xml,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Core,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Gui,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::Network,INTERFACE_INCLUDE_DIRECTORIES>

    $<TARGET_PROPERTY:KF${QT_VERSION_MAJOR}::I18n,INTERFACE_INCLUDE_DIRECTORIES>
)

set(libllmengine_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/llmprocesssettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/llmprocessmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/llmresponsehelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/koboldcpp/llmprocessmanagerkoboldcpp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/installer/llmengineinstaller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/installer/llmengineinstalldialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/installer/llmengineinstallaction.cpp
)

# Used by digikamcore
add_library(core_digikamllmengine_obj OBJECT ${libllmengine_SRCS})

target_compile_definitions(core_digikamllmengine_obj
                           PRIVATE
                           digikamcore_EXPORTS
                           DIGIKAMCORE_EXPORT=DIGIKAM_CORE_EXPORT
)

# For unit-tests
add_library(digikamllmengine STATIC $<TARGET_OBJECTS:core_digikamllmengine_obj>)

# Install llama.cpp installer scripts
install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/scripts/llamacpp_linux.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/scripts/llamacpp_macos.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/scripts/llamacpp_win.bat"
        DESTINATION ${DATA_INSTALL_DIR}/digikam/llm_install_scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
)

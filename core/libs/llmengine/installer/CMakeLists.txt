#
# SPDX-FileCopyrightText: 2010-2025 by Gilles Caulier, <caulier dot gilles at gmail dot com>
# SPDX-FileCopyrightText: 2025      by Michael Miller, <michael underscore miller at msn dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

install(DIRECTORY scripts/
        DESTINATION ${KDE_INSTALL_FULL_DATADIR}/digikam/llm_install_scripts
)

set(installertest_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/installer/installertest.cpp
)

add_executable(llamacpp_installer_test ${installertest_SRCS})

target_link_libraries(llamacpp_installer_test
    digikamcore
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

# Make sure the executable can find the scripts
set_target_properties(llamacpp_installer_test PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR}/digikam/installer"
)

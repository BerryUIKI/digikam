# CMake file for the LlamaCppInstaller test application

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

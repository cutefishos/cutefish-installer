cmake_minimum_required(VERSION 3.16)

project(cutefish-installer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

find_package(CutefishToolkit REQUIRED)

add_subdirectory(common)
add_subdirectory(daemon)
add_subdirectory(src)

install(FILES data/logo.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/cutefish-installer)

cutefish_add_translations(cutefish-installer SOURCES src
                          DESKTOP_FILES data/cutefish-installer.desktop data/cutefish-installer-autostart.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cutefish-installer.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cutefish-installer-autostart.desktop
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart
        RENAME cutefish-installer.desktop)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

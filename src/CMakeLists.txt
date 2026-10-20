add_library(installer-ui STATIC
    backend.cpp
    backend.h
    components.cpp
    components.h
    installerwindow.cpp
    installerwindow.h
    pages.cpp
    pages.h
    partitionpage.cpp
    partitionpage.h
    timezones.cpp
    timezones.h
)
target_include_directories(installer-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(installer-ui PRIVATE
    INSTALLER_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/cutefish-installer")
target_link_libraries(installer-ui PUBLIC installer-common Cutefish::Toolkit)

add_executable(cutefish-installer main.cpp)
target_compile_definitions(cutefish-installer PRIVATE INSTALLER_VERSION="${PROJECT_VERSION}")
target_link_libraries(cutefish-installer PRIVATE installer-ui)

install(TARGETS cutefish-installer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

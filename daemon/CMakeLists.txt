add_library(installer-service STATIC
    disks.cpp
    disks.h
    installerservice.cpp
    installerservice.h
    installjob.cpp
    installjob.h
    plan.cpp
    plan.h
)
target_include_directories(installer-service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(installer-service PUBLIC installer-common)

add_executable(cutefish-installer-service main.cpp)
target_link_libraries(cutefish-installer-service PRIVATE installer-service)

configure_file(org.cutefish.Installer.service.in org.cutefish.Installer.service @ONLY)
configure_file(cutefish-installer.service.in cutefish-installer.service @ONLY)

install(TARGETS cutefish-installer-service DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES org.cutefish.Installer.conf DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/system.d)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.cutefish.Installer.service
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/system-services)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cutefish-installer.service DESTINATION lib/systemd/system)
install(FILES org.cutefish.installer.policy DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/polkit-1/actions)

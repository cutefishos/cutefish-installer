add_library(installer-common STATIC
    installer.cpp
    installer.h
)
target_include_directories(installer-common PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(installer-common PUBLIC Cutefish::ToolkitBase)

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(installer-tests
    disks_test.cpp
    installer_test.cpp
    partitioning_test.cpp
    plan_test.cpp
    timezones_test.cpp
)
target_link_libraries(installer-tests PRIVATE installer-service installer-ui GTest::gtest_main)
gtest_discover_tests(installer-tests)

add_executable(installer-capture capture.cpp)
target_link_libraries(installer-capture PRIVATE installer-ui)

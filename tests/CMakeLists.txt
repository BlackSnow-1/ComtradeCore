# 1. 扫描 tests 目录下的所有 cpp 文件
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

# 2. 只有当确实找到了测试文件时，才创建可执行目标
if(TEST_SOURCES)
    add_executable(comtrade_unit_tests ${TEST_SOURCES})

    target_link_libraries(comtrade_unit_tests PRIVATE
            comtrade::ComtradeCore
            GTest::gtest
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(comtrade_unit_tests)

    message(STATUS "ComtradeCore: Unit tests enabled.")
else()
    message(STATUS "ComtradeCore: No test files found in tests/ directory. Skipping unit tests.")
endif()
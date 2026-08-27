# 1. 扫描 tests 目录下的所有 cpp 文件
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")
list(FILTER TEST_SOURCES EXCLUDE REGEX "test_real_files\\.cpp$")

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

set(COMTRADE_REAL_FILE_TEST "${CMAKE_CURRENT_SOURCE_DIR}/test_real_files.cpp")
if(EXISTS "${COMTRADE_REAL_FILE_TEST}")
    add_executable(comtrade_accuracy_tests "${COMTRADE_REAL_FILE_TEST}")
    target_link_libraries(comtrade_accuracy_tests PRIVATE
            comtrade::ComtradeCore
            GTest::gtest
            GTest::gtest_main
    )
    target_compile_definitions(comtrade_accuracy_tests PRIVATE
            COMTRADE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/ComtradeFiles"
    )

    include(GoogleTest)
    gtest_discover_tests(comtrade_accuracy_tests
            PROPERTIES LABELS "accuracy"
    )
endif()

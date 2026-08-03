cmake_minimum_required(VERSION 3.14)
project(ComtradeCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# 1. 核心库配置 (Header-only)
# ==========================================
add_library(ComtradeCore INTERFACE
        include/comtrade/record.hpp)
add_library(comtrade::ComtradeCore ALIAS ComtradeCore)

target_include_directories(ComtradeCore INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ==========================================
# 2. 单元测试配置 (引入 GoogleTest)
# ==========================================
# 开启 CTest 测试支持
enable_testing()

# 优先查找本地系统是否已安装 GTest (QUIET 表示找不到时不报错)
find_package(GTest QUIET)

if(GTest_FOUND)
    message(STATUS "Found local GoogleTest, using local version.")
else()
    message(STATUS "Local GoogleTest not found. Fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0 # 使用较新的稳定版本
    )
    # 对于 Windows MSVC 避免运行时库冲突
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# 创建测试目标（注意：这里只包含测试文件）
add_executable(test_parser tests/test_record.cpp)

# 链接核心库与 gtest_main (gtest_main 自带 main 函数)
target_link_libraries(test_parser PRIVATE
        comtrade::ComtradeCore
        GTest::gtest_main
)

# 使用 CMake 自动发现并注册 GoogleTest 宏定义的测试用例
include(GoogleTest)
gtest_discover_tests(test_parser)


option(COMTRADE_BUILD_EXAMPLES "Build ComtradeCore examples" ON)

if(COMTRADE_BUILD_EXAMPLES)
    message(STATUS "ComtradeCore: Building examples enabled")
    add_subdirectory(examples)
endif()
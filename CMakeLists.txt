cmake_minimum_required(VERSION 3.14)
project(ComtradeCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# 1. 核心库配置 (Header-only)
# ==========================================
# 1. 【必须在最前面】先真正创建这个 INTERFACE 库
add_library(ComtradeCore INTERFACE)

# 2. 【第二步】为它创建一个带命名空间的别名 (ALIAS)，方便后续引用
add_library(comtrade::ComtradeCore ALIAS ComtradeCore)

# 3. 【第三步】为真正的库配置头文件搜索路径
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
option(COMTRADE_BUILD_TESTS "Build ComtradeCore unit tests" ON)

if(COMTRADE_BUILD_TESTS)
    enable_testing()

    find_package(GTest QUIET)
    if(GTest_FOUND)
        message(STATUS "Found local GoogleTest, using local version.")
    else()
        message(STATUS "Local GoogleTest not found. Fetching from GitHub...")
        include(FetchContent)
        FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG        v1.14.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    # 重点：不要在这里扫描 cpp 文件，而是去加载 tests 目录的 CMake
    add_subdirectory(tests)
endif()


option(COMTRADE_BUILD_EXAMPLES "Build ComtradeCore examples" ON)

if(COMTRADE_BUILD_EXAMPLES)
    message(STATUS "ComtradeCore: Building examples enabled")
    add_subdirectory(examples)
endif()
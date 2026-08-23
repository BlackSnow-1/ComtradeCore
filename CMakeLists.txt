cmake_minimum_required(VERSION 3.14)
project(ComtradeCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(COMTRADE_IS_TOP_LEVEL ON)
else()
    set(COMTRADE_IS_TOP_LEVEL OFF)
endif()

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
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(ComtradeCore INTERFACE cxx_std_17)

# ==========================================
# 2. 构建选项配置
# ==========================================
option(COMTRADE_BUILD_TESTS "Build ComtradeCore unit tests" ${COMTRADE_IS_TOP_LEVEL})
option(COMTRADE_BUILD_EXAMPLES "Build ComtradeCore examples" ${COMTRADE_IS_TOP_LEVEL})
# --- 新增的多语言绑定选项 ---
option(BUILD_JAVA_BINDINGS "Build Java bindings using SWIG" OFF)

# ==========================================
# 3. 子目录引入配置
# ==========================================
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

if(COMTRADE_BUILD_EXAMPLES)
    message(STATUS "ComtradeCore: Building examples enabled")
    add_subdirectory(examples)
endif()

# --- 条件引入绑定子模块 ---
if(BUILD_JAVA_BINDINGS)
    message(STATUS "ComtradeCore: Building Java bindings enabled")
    add_subdirectory(bindings/java)
endif()

# ==========================================
# 4. 安装与 CMake package 导出
# ==========================================
install(TARGETS ComtradeCore
        EXPORT ComtradeCoreTargets
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT ComtradeCoreTargets
        FILE ComtradeCoreTargets.cmake
        NAMESPACE comtrade::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ComtradeCore
)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ComtradeCoreConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/ComtradeCoreConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ComtradeCore
)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/ComtradeCoreConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ComtradeCoreConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/ComtradeCoreConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ComtradeCore
)

install(FILES LICENSE README.md
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/ComtradeCore
)
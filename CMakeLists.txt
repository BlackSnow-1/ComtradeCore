cmake_minimum_required(VERSION 3.14)
project(ComtradeCore VERSION 1.2.0.0 LANGUAGES CXX)

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

# Linux 通常由 libc 提供 iconv，macOS 通常需要显式链接系统 libiconv。
# 使用导入目标可以让 CMake 为两种情况传播正确的头文件和链接参数。
if(UNIX)
    find_package(Iconv REQUIRED)
    target_link_libraries(ComtradeCore INTERFACE Iconv::Iconv)
endif()

# ==========================================
# 2. 构建选项配置
# ==========================================
option(COMTRADE_BUILD_TESTS "Build ComtradeCore unit tests" ${COMTRADE_IS_TOP_LEVEL})
option(COMTRADE_BUILD_EXAMPLES "Build ComtradeCore examples" ${COMTRADE_IS_TOP_LEVEL})
option(COMTRADE_BUILD_BENCHMARKS "Build ComtradeCore benchmarks" OFF)
# --- 新增的多语言绑定选项 ---
option(BUILD_JAVA_BINDINGS "Build Java bindings using SWIG" OFF)
option(COMTRADE_BUILD_AI "Build optional C++ AI analysis library and CLI" OFF)
# 只在 COMTRADE_BUILD_AI=ON 时生效：是否把 AI 模块（include/comtrade/ai/、comtrade-ai CLI、
# 第三方依赖 license）纳入 `cmake --install`。关闭时仍会构建并测试 AI 模块，只是不安装它，
# 用于只想在本地构建/跑测试、但发布包不希望携带 AI 及其依赖头文件的场景。
option(COMTRADE_INSTALL_AI "Install the optional AI module (headers, CLI, licenses) when COMTRADE_BUILD_AI is ON" ON)

# ==========================================
# 3. 子目录引入配置
# ==========================================
# AI 模块（如果启用）必须先于 tests 加入：tests/CMakeLists.txt 里的 AI 单测目标要链接
# comtrade::ComtradeAI，该 target 由 analysis/cpp 定义。
if(COMTRADE_BUILD_AI)
    add_subdirectory(analysis/cpp)
endif()

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

if(COMTRADE_BUILD_BENCHMARKS)
    message(STATUS "ComtradeCore: Building benchmarks enabled")
    add_subdirectory(benchmarks)
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

# AI 模块的头文件（include/comtrade/ai.hpp 和 include/comtrade/ai/）物理上单独放在自己的
# 子目录里，安装与否由 analysis/cpp/CMakeLists.txt 按 COMTRADE_BUILD_AI/COMTRADE_INSTALL_AI
# 单独控制，这里的核心库安装规则不涉及它们。
install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        REGEX "/comtrade/ai($|\\.hpp|/.*)$" EXCLUDE
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

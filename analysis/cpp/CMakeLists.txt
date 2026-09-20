cmake_minimum_required(VERSION 3.18)
include(FetchContent)

find_package(OpenSSL 3.0 REQUIRED)
find_package(Threads REQUIRED)

# Each dependency below is first looked up on the system (a distro package, vcpkg, a manually
# installed copy, ...); FetchContent only runs as a fallback when nothing suitable is found. This
# keeps CI/offline/reproducible builds working exactly as before (nothing is ever silently upgraded
# or downgraded from the pinned commit) while letting a normal developer machine or Linux container
# that already has the library installed skip the git clone + build entirely.

# nlohmann/json 3.12.0 (header-only) — https://github.com/nlohmann/json
find_package(nlohmann_json 3.12 QUIET CONFIG)
if(nlohmann_json_FOUND)
    message(STATUS "ComtradeCore AI: found local nlohmann/json ${nlohmann_json_VERSION}, using it.")
else()
    message(STATUS "ComtradeCore AI: local nlohmann/json >= 3.12 not found. Fetching v3.12.0 from GitHub...")
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install ON CACHE BOOL "" FORCE)
    FetchContent_Declare(ai_json GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG 55f93686c01528224f448c19128836e7df245f72) # v3.12.0
    FetchContent_MakeAvailable(ai_json)
    set(COMTRADE_AI_JSON_FETCHED ON)
endif()

# cpp-httplib 0.28.0 (header-only HTTP/HTTPS client+server) — https://github.com/yhirose/cpp-httplib
# Its own CMakeLists.txt (used both when found on the system and when fetched) provides the
# httplib::httplib INTERFACE target: include directories, the CPPHTTPLIB_OPENSSL_SUPPORT compile
# definition and OpenSSL linkage all come from linking that target, so ComtradeAI never has to
# duplicate them.
find_package(httplib 0.28 QUIET CONFIG)
if(httplib_FOUND)
    message(STATUS "ComtradeCore AI: found local cpp-httplib ${httplib_VERSION}, using it.")
else()
    message(STATUS "ComtradeCore AI: local cpp-httplib >= 0.28 not found. Fetching v0.28.0 from GitHub...")
    set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "" FORCE)
    set(HTTPLIB_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(ai_http GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG adf58bf474fac638160592d6c3f67da4ebc7df20) # v0.28.0
    FetchContent_MakeAvailable(ai_http)
    set(COMTRADE_AI_HTTP_FETCHED ON)
endif()

# libharu 2.4.4 (pure C PDF generation, no Qt) — https://github.com/libharu/libharu
# libharu ships no CMake package config of its own (no `install(EXPORT ...)`), so a plain
# find_package() cannot locate a system install; find_path()/find_library() is the closest
# equivalent. Unlike the two dependencies above, ai/pdf.hpp reaches into libharu's *internal*
# HPDF_FontAttr struct layout as a documented workaround for a ToUnicode CMap bug specific to the
# 2.4.x line (see the comment in include/comtrade/ai/pdf.hpp) — that is not part of libharu's public
# API/ABI contract, so a local install is only accepted when its advertised version is also 2.4.x;
# anything else falls back to fetching the exact pinned build to avoid silently miscompiling against
# an incompatible internal layout.
find_path(COMTRADE_HPDF_INCLUDE_DIR NAMES hpdf.h)
find_library(COMTRADE_HPDF_LIBRARY NAMES hpdf)
set(COMTRADE_HPDF_LOCAL_USABLE OFF)
if(COMTRADE_HPDF_INCLUDE_DIR AND COMTRADE_HPDF_LIBRARY)
    file(STRINGS "${COMTRADE_HPDF_INCLUDE_DIR}/hpdf_version.h" COMTRADE_HPDF_VERSION_H
        REGEX "HPDF_(MAJOR|MINOR)_VERSION")
    string(REGEX MATCH "HPDF_MAJOR_VERSION[ \t]+([0-9]+)" _ "${COMTRADE_HPDF_VERSION_H}")
    set(COMTRADE_HPDF_MAJOR "${CMAKE_MATCH_1}")
    string(REGEX MATCH "HPDF_MINOR_VERSION[ \t]+([0-9]+)" _ "${COMTRADE_HPDF_VERSION_H}")
    set(COMTRADE_HPDF_MINOR "${CMAKE_MATCH_1}")
    if(COMTRADE_HPDF_MAJOR STREQUAL "2" AND COMTRADE_HPDF_MINOR STREQUAL "4")
        set(COMTRADE_HPDF_LOCAL_USABLE ON)
    else()
        message(STATUS "ComtradeCore AI: found libharu ${COMTRADE_HPDF_MAJOR}.${COMTRADE_HPDF_MINOR} at "
            "${COMTRADE_HPDF_LIBRARY}, but only the 2.4.x line is ABI-compatible with the internal "
            "font structures this module touches; ignoring it.")
    endif()
endif()
if(COMTRADE_HPDF_LOCAL_USABLE)
    message(STATUS "ComtradeCore AI: found local libharu 2.4 at ${COMTRADE_HPDF_LIBRARY}, using it.")
    if(NOT TARGET hpdf)
        add_library(hpdf UNKNOWN IMPORTED)
        set_target_properties(hpdf PROPERTIES
            IMPORTED_LOCATION "${COMTRADE_HPDF_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${COMTRADE_HPDF_INCLUDE_DIR}")
    endif()
else()
    message(STATUS "ComtradeCore AI: no ABI-compatible local libharu 2.4.x found. Fetching v2.4.4 from GitHub...")
    # Keep it isolated from the rest of the header-only core: build it as a static library with its
    # optional PNG/Zlib image support disabled, since the AI module only needs text rendering.
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_DISABLE_FIND_PACKAGE_PNG ON)
    set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB ON)
    set(LIBHPDF_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(ai_pdf GIT_REPOSITORY https://github.com/libharu/libharu.git
        GIT_TAG 0c598becaadaef8e3d12b883f9fc2864a118c12d) # v2.4.4
    FetchContent_MakeAvailable(ai_pdf)
    set(COMTRADE_AI_PDF_FETCHED ON)
endif()

# ComtradeAI is header-only (include/comtrade/ai.hpp + include/comtrade/ai/*.hpp, physically
# separate from the rest of the header-only core in include/comtrade/*.hpp): this target only
# declares the usage requirements those headers need (include paths, compile definitions, link
# libraries) and compiles nothing of its own. When libharu was fetched (rather than found on the
# system), it has no CMake package config of its own, so its `hpdf` target is folded into our own
# ComtradeAITargets export below, alongside comtrade::ComtradeAI and the CLI.
add_library(ComtradeAI INTERFACE)
add_library(comtrade::ComtradeAI ALIAS ComtradeAI)
target_link_libraries(ComtradeAI INTERFACE
    comtrade::ComtradeCore
    nlohmann_json::nlohmann_json
    httplib::httplib
    hpdf
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads)
if(COMTRADE_AI_PDF_FETCHED)
    # A fetched libharu only registers its include directories via the legacy directory-scoped
    # include_directories(), which does not propagate outside its own FetchContent subdirectory; a
    # found system install already carries its own INTERFACE_INCLUDE_DIRECTORIES (set above), and
    # the installed headers are covered separately by comtrade::ComtradeCore's own INSTALL_INTERFACE.
    target_include_directories(ComtradeAI INTERFACE
        $<BUILD_INTERFACE:${ai_pdf_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${ai_pdf_BINARY_DIR}/include>)
endif()
if(WIN32)
    target_link_libraries(ComtradeAI INTERFACE ws2_32 crypt32)
endif()

add_executable(comtrade-ai src/main.cpp)
target_link_libraries(comtrade-ai PRIVATE ComtradeAI)
if(MSVC)
    target_compile_options(comtrade-ai PRIVATE /utf-8)
endif()

# COMTRADE_INSTALL_AI is the dedicated install-time switch for this module: with it OFF, AI targets
# above are still built and tested locally, but `cmake --install` neither ships its headers/CLI nor
# exports comtrade::ComtradeAI, so a downstream find_package(ComtradeCore COMPONENTS AI) correctly
# reports it as not available (see ComtradeAI_FOUND handling in cmake/ComtradeCoreConfig.cmake.in).
if(COMTRADE_INSTALL_AI)
    install(TARGETS ComtradeAI EXPORT ComtradeAITargets)
    install(TARGETS comtrade-ai EXPORT ComtradeAITargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(COMTRADE_AI_PDF_FETCHED)
        install(TARGETS hpdf EXPORT ComtradeAITargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(EXPORT ComtradeAITargets NAMESPACE comtrade:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ComtradeCore)

    # AI 头文件单独安装：include/comtrade/ai.hpp（入口）与整个 include/comtrade/ai/ 子目录，
    # 与核心头文件的安装规则（顶层 CMakeLists.txt）完全分开。
    install(FILES ${CMAKE_SOURCE_DIR}/include/comtrade/ai.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/comtrade)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/comtrade/ai DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/comtrade)

    install(FILES ${CMAKE_SOURCE_DIR}/config/ai-config.example.json ${CMAKE_SOURCE_DIR}/docs/ai-analysis.md
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/ComtradeCore)
    # A locally found dependency's license already ships with that install (e.g. the distro
    # package's own copyright file); only bundle the license text ourselves for what we fetched.
    if(COMTRADE_AI_HTTP_FETCHED)
        install(FILES ${ai_http_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/ComtradeCore/licenses RENAME cpp-httplib.txt)
    endif()
    if(COMTRADE_AI_JSON_FETCHED)
        install(FILES ${ai_json_SOURCE_DIR}/LICENSE.MIT DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/ComtradeCore/licenses RENAME nlohmann-json.txt)
    endif()
    if(COMTRADE_AI_PDF_FETCHED)
        install(FILES ${ai_pdf_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/ComtradeCore/licenses RENAME libharu.txt)
    endif()
endif()

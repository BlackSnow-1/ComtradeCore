# Finds a locally installed libharu 2.4.x and defines an `hpdf` IMPORTED target for it.
#
# libharu ships no CMake package config of its own (no `install(EXPORT ...)`), so plain
# find_package() cannot locate a system install; find_path()/find_library() is the closest
# equivalent. This module is shared by two call sites that must resolve libharu the exact same way:
#   - analysis/cpp/CMakeLists.txt, when configuring ComtradeCore's own AI module;
#   - cmake/ComtradeCoreConfig.cmake.in, when a downstream project does
#     find_package(ComtradeCore COMPONENTS AI) against an install that itself used a local libharu
#     (see ComtradeCore_AI_HPDF_FETCHED there). A bare `hpdf` target cannot be exported by CMake
#     (IMPORTED targets cannot be part of an install(EXPORT ...) set), so the consumer must redefine
#     an equivalent `hpdf` target on their own machine before comtrade::ComtradeAI's link interface
#     (which references it by that bare name) can resolve.
#
# include/comtrade/ai/pdf.hpp reaches into libharu's *internal* HPDF_FontAttr struct layout as a
# documented workaround for a ToUnicode CMap bug specific to the 2.4.x line, so only that exact
# minor version line is accepted here; anything else is treated as not found, and the caller should
# fall back to fetching the pinned build instead of risking an ABI mismatch on those internals.
function(comtrade_find_hpdf)
    if(TARGET hpdf)
        set(COMTRADE_HPDF_FOUND TRUE PARENT_SCOPE)
        return()
    endif()

    find_path(COMTRADE_HPDF_INCLUDE_DIR NAMES hpdf.h)
    find_library(COMTRADE_HPDF_LIBRARY NAMES hpdf)
    set(COMTRADE_HPDF_FOUND FALSE PARENT_SCOPE)
    if(NOT COMTRADE_HPDF_INCLUDE_DIR OR NOT COMTRADE_HPDF_LIBRARY)
        return()
    endif()

    file(STRINGS "${COMTRADE_HPDF_INCLUDE_DIR}/hpdf_version.h" _comtrade_hpdf_version_h
        REGEX "HPDF_(MAJOR|MINOR)_VERSION")
    string(REGEX MATCH "HPDF_MAJOR_VERSION[ \t]+([0-9]+)" _ "${_comtrade_hpdf_version_h}")
    set(_comtrade_hpdf_major "${CMAKE_MATCH_1}")
    string(REGEX MATCH "HPDF_MINOR_VERSION[ \t]+([0-9]+)" _ "${_comtrade_hpdf_version_h}")
    set(_comtrade_hpdf_minor "${CMAKE_MATCH_1}")
    if(NOT _comtrade_hpdf_major STREQUAL "2" OR NOT _comtrade_hpdf_minor STREQUAL "4")
        message(STATUS "ComtradeCore AI: found libharu ${_comtrade_hpdf_major}.${_comtrade_hpdf_minor} at "
            "${COMTRADE_HPDF_LIBRARY}, but only the 2.4.x line is ABI-compatible with the internal "
            "font structures this module touches; ignoring it.")
        return()
    endif()

    add_library(hpdf UNKNOWN IMPORTED)
    set_target_properties(hpdf PROPERTIES
        IMPORTED_LOCATION "${COMTRADE_HPDF_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${COMTRADE_HPDF_INCLUDE_DIR}")
    set(COMTRADE_HPDF_FOUND TRUE PARENT_SCOPE)
endfunction()

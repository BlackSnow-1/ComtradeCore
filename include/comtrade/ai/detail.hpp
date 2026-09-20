/**
 * @file detail.hpp
 * @brief Internal helpers shared by the header-only AI analysis module. Not part of the public API;
 *        names under comtrade::ai::detail may change without notice.
 */
#pragma once

#include <stdexcept>
#include <string>

namespace comtrade::ai::detail {

// Throws std::runtime_error(message) when the condition is false. Used throughout the AI module so
// every failure path (bad config, network error, malformed response, ...) raises one clear,
// catchable exception instead of an assertion, a crash, or a silently wrong result.
inline void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}

// User-controlled strings that end up in an HTTP header, a bearer token or an error message must
// not contain CR/LF/NUL: those bytes could inject an extra header line or truncate a C string.
inline bool isClean(const std::string &value) {
    return value.find_first_of(std::string("\r\n\0", 3)) == std::string::npos;
}

} // namespace comtrade::ai::detail

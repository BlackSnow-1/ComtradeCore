#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <iconv.h>
#endif

namespace comtrade::detail {

inline bool isValidUtf8(const std::string& text) noexcept {
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const auto first = static_cast<std::uint8_t>(text[cursor]);
        if (first <= 0x7FU) {
            ++cursor;
            continue;
        }

        std::size_t continuation_count = 0;
        std::uint32_t code_point = 0;
        std::uint32_t minimum = 0;
        if (first >= 0xC2U && first <= 0xDFU) {
            continuation_count = 1;
            code_point = first & 0x1FU;
            minimum = 0x80U;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            continuation_count = 2;
            code_point = first & 0x0FU;
            minimum = 0x800U;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            continuation_count = 3;
            code_point = first & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }

        if (cursor + continuation_count >= text.size()) return false;
        for (std::size_t index = 1; index <= continuation_count; ++index) {
            const auto next = static_cast<std::uint8_t>(text[cursor + index]);
            if ((next & 0xC0U) != 0x80U) return false;
            code_point = (code_point << 6U) | (next & 0x3FU);
        }
        if (code_point < minimum || code_point > 0x10FFFFU ||
            (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
            return false;
        }
        cursor += continuation_count + 1;
    }
    return true;
}

inline bool gb18030ToUtf8(const std::string& input, std::string& output) {
    if (input.empty()) {
        output.clear();
        return true;
    }

#if defined(_WIN32)
    constexpr UINT gb18030_code_page = 54936;
    const int wide_size = MultiByteToWideChar(
        gb18030_code_page, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (wide_size <= 0) return false;

    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    if (MultiByteToWideChar(gb18030_code_page, MB_ERR_INVALID_CHARS, input.data(),
                            static_cast<int>(input.size()), wide.data(), wide_size) != wide_size) {
        return false;
    }

    const int utf8_size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), wide_size, nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0) return false;

    output.resize(static_cast<std::size_t>(utf8_size));
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), wide_size,
                               output.data(), utf8_size, nullptr, nullptr) == utf8_size;
#elif defined(__linux__)
    iconv_t converter = iconv_open("UTF-8", "GB18030");
    if (converter == reinterpret_cast<iconv_t>(-1)) return false;

    std::string converted(input.size() * 2U + 16U, '\0');
    char* input_cursor = const_cast<char*>(input.data());
    std::size_t input_remaining = input.size();
    std::size_t produced = 0;

    while (true) {
        char* output_cursor = converted.data() + produced;
        std::size_t output_remaining = converted.size() - produced;
        const auto result = iconv(
            converter, &input_cursor, &input_remaining, &output_cursor, &output_remaining);
        produced = converted.size() - output_remaining;
        if (result != static_cast<std::size_t>(-1)) break;
        if (errno != E2BIG) {
            iconv_close(converter);
            return false;
        }
        converted.resize(converted.size() * 2U);
    }

    iconv_close(converter);
    converted.resize(produced);
    output = std::move(converted);
    return true;
#else
    (void)output;
    return false;
#endif
}

inline void normalizeCfgLinesToUtf8(std::vector<std::string>& lines) {
    if (!lines.empty() && lines.front().size() >= 3 &&
        static_cast<std::uint8_t>(lines.front()[0]) == 0xEFU &&
        static_cast<std::uint8_t>(lines.front()[1]) == 0xBBU &&
        static_cast<std::uint8_t>(lines.front()[2]) == 0xBFU) {
        lines.front().erase(0, 3);
    }

    bool utf8 = true;
    for (const auto& line : lines) {
        if (!isValidUtf8(line)) {
            utf8 = false;
            break;
        }
    }
    if (utf8) return;

    std::vector<std::string> converted_lines;
    converted_lines.reserve(lines.size());
    for (const auto& line : lines) {
        std::string converted;
        if (!gb18030ToUtf8(line, converted)) {
            return; // Unknown encoding: retain the original byte-preserving behavior.
        }
        converted_lines.push_back(std::move(converted));
    }
    lines = std::move(converted_lines);
}

} // namespace comtrade::detail

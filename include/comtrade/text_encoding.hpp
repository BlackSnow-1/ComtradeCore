/**
 * @file text_encoding.hpp
 * @brief CFG 文本编码检测，以及 GB18030/GBK/GB2312 到 UTF-8 的平台转换。
 */
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
#elif defined(__linux__) || defined(__APPLE__)
#include <iconv.h>
#endif

namespace comtrade::detail {

// 严格校验 UTF-8，拒绝截断序列、过长编码、代理项和超出 Unicode 范围的码点。
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

// GB18030 是 GBK/GB2312 的超集，选择它可以用一个转换入口覆盖常见国产录波器编码。
inline bool gb18030ToUtf8(const std::string& input, std::string& output) {
    if (input.empty()) {
        output.clear();
        return true;
    }

#if defined(_WIN32)
    // Windows 代码页 54936 对应 GB18030，先转 UTF-16 再编码为 UTF-8。
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
#elif defined(__linux__) || defined(__APPLE__)
    // Linux 和 macOS 都通过 iconv 转码；输出缓冲不足时按需扩容，不截断多字节字符。
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

// 转换采用“全有或全无”：任意一行无法按 GB18030 解码时保留整份原始内容，
// 避免同一份 CFG 中一部分转换、一部分未转换。合法 UTF-8 始终优先。
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

    // 先写入临时容器，全部成功后再替换调用方数据。
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

/**
* @file utils.hpp
 * @brief Internal utility functions for string processing.
 * @author 王广博
 */
#pragma once

#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <ctime>

#include "types.hpp"

namespace comtrade::utils {
    // COMTRADE 是逗号分隔格式；这里同时清除 CRLF 残留和字段两端空白。
    // 使用 inline 保证该实现可安全放在 Header-only 库中。
    inline std::vector<std::string> split(const std::string &s, const char delimiter = ',') {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            // 去除首尾的空格和回车符 (\r)，兼容 Windows/Unix 换行
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            tokens.push_back(token);
        }
        if (!s.empty() && s.back() == delimiter) tokens.emplace_back();
        return tokens;
    }

    // 未明确标识 1999/2013 的厂商版本按 1991 主体布局处理，尾部扩展由 CFG 解析器兼容。
    inline StandardVersion determineVersion(const std::string &year_str) {
        if (year_str.find("1999") != std::string::npos) return StandardVersion::V1999;
        if (year_str.find("2013") != std::string::npos) return StandardVersion::V2013;
        return StandardVersion::V1991;
    }

    /**
     * @brief 将纳秒级 TimePoint 格式化为 COMTRADE 时间字符串
     * @return 格式为: dd/mm/yyyy,hh:mm:ss.sssssssss
     */
    inline std::string formatTime(const TimePoint &tp, uint8_t fractional_digits = 9, bool utc = false, int offset_minutes = 0) {
        // 先拆分整秒和纳秒余数，避免浮点格式化造成精度损失。
        const auto display = tp + std::chrono::minutes(utc ? offset_minutes : 0);
        auto secs = std::chrono::time_point_cast<std::chrono::seconds>(display);
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(display - secs).count();
        if (ns < 0) { secs -= std::chrono::seconds(1); ns += 1000000000LL; }

        std::time_t t = std::chrono::system_clock::to_time_t(secs);
        std::tm tm_buf;

        // CFG 日期字段使用运行机器的本地时区；2013 的时区代码由 CfgData 单独保存。
#if defined(_WIN32)
        if (utc) gmtime_s(&tm_buf, &t); else localtime_s(&tm_buf, &t);
#else
        if (utc) gmtime_r(&t, &tm_buf); else localtime_r(&t, &tm_buf);
#endif

        char buf[32];
        // 格式化基础时间 dd/mm/yyyy,hh:mm:ss
        std::strftime(buf, sizeof(buf), "%d/%m/%Y,%H:%M:%S", &tm_buf);

        const auto digits = std::min<uint8_t>(fractional_digits, 9);
        std::ostringstream oss;
        oss << buf;
        if (digits > 0) {
            static constexpr int64_t divisors[] = {
                1000000000LL, 100000000LL, 10000000LL, 1000000LL, 100000LL,
                10000LL, 1000LL, 100LL, 10LL, 1LL
            };
            const auto fraction = ns / divisors[digits];
            oss << "." << std::setfill('0') << std::setw(digits) << fraction;
        }
        return oss.str();
    }

    // 统计输入中实际出现的小数秒位数，最多保留到库支持的纳秒级 9 位。
    inline uint8_t fractionalSecondDigits(const std::string& time_str) {
        const auto dot = time_str.find('.');
        if (dot == std::string::npos) return 0;

        uint8_t digits = 0;
        for (auto i = dot + 1; i < time_str.size() && digits < 9; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(time_str[i]))) break;
            ++digits;
        }
        return digits;
    }

    /**
     * @brief 解析 COMTRADE 时间字符串为纳秒级 TimePoint (纯流提取安全版)
     */
    // Accept the standard signed hours / HhMM and common vendor HHMM / HH:MM forms.
    inline bool parseUtcOffset(const std::string& text, int& minutes) {
        if (text == "UTC" || text == "utc" || text == "Z") { minutes = 0; return true; }
        std::smatch match;
        static const std::regex pattern(R"(^([+-]?)([0-9]{1,2})(?:[hH:]([0-9]{2}))?$)");
        int hours = 0, mins = 0, sign = 1;
        if (std::regex_match(text, match, pattern)) {
            sign = match[1] == "-" ? -1 : 1;
            hours = std::stoi(match[2]);
            if (match[3].matched) mins = std::stoi(match[3]);
        } else {
            static const std::regex compact(R"(^([+-])([0-9]{2})([0-9]{2})$)");
            if (!std::regex_match(text, match, compact)) return false;
            sign = match[1] == "-" ? -1 : 1;
            hours = std::stoi(match[2]); mins = std::stoi(match[3]);
        }
        if (hours > 23 || mins > 59) return false;
        minutes = sign * (hours * 60 + mins); return true;
    }

    inline TimePoint parseTime(const std::string& text, bool utc = false, int offset_minutes = 0) {
        std::smatch m;
        static const std::regex pattern(
            R"(^([0-9]{1,2})/([0-9]{1,2})/([0-9]{4}),([0-9]{1,2}):([0-9]{2}):([0-9]{2})(?:\.([0-9]{1,9}))?\s*$)");
        if (!std::regex_match(text, m, pattern)) throw std::invalid_argument("Invalid CFG date/time");
        const int day = std::stoi(m[1]), month = std::stoi(m[2]), year = std::stoi(m[3]);
        const int hour = std::stoi(m[4]), minute = std::stoi(m[5]), second = std::stoi(m[6]);
        static constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        if (year < 1678 || year > 2261 || month < 1 || month > 12 || day < 1 ||
            day > days[month-1] + (month == 2 && leap) || hour > 23 || minute > 59 || second > 59)
            throw std::invalid_argument("CFG date/time out of range");
        std::tm tm{}; tm.tm_year = year-1900; tm.tm_mon = month-1; tm.tm_mday = day;
        tm.tm_hour = hour; tm.tm_min = minute; tm.tm_sec = second; tm.tm_isdst = -1;
        std::time_t seconds;
#if defined(_WIN32)
        seconds = utc ? _mkgmtime(&tm) : std::mktime(&tm);
#else
        seconds = utc ? timegm(&tm) : std::mktime(&tm);
#endif
        std::string fraction = m[7].matched ? m[7].str() : "";
        fraction.append(9 - fraction.size(), '0');
        return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::from_time_t(seconds))
             + std::chrono::nanoseconds(std::stoll(fraction)) - std::chrono::minutes(utc ? offset_minutes : 0);
    }
}

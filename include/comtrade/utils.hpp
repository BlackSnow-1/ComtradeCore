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

#include "types.hpp"

namespace comtrade::utils {
    // 使用 inline 关键字保证 header-only 兼容性
    inline std::vector<std::string> split(const std::string &s, const char delimiter = ',') {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            // 去除首尾的空格和回车符 (\r)，兼容 Windows/Unix 换行
            token.erase(0, token.find_first_not_of(" \r\n"));
            token.erase(token.find_last_not_of(" \r\n") + 1);
            tokens.push_back(token);
        }
        return tokens;
    }

    inline StandardVersion determineVersion(const std::string &year_str) {
        if (year_str.find("1999") != std::string::npos) return StandardVersion::V1999;
        if (year_str.find("2013") != std::string::npos) return StandardVersion::V2013;
        return StandardVersion::V1991;
    }

    /**
     * @brief 将纳秒级 TimePoint 格式化为 COMTRADE 时间字符串
     * @return 格式为: dd/mm/yyyy,hh:mm:ss.sssssssss
     */
    inline std::string formatTime(const TimePoint &tp, uint8_t fractional_digits = 9) {
        // 提取秒级部分
        auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
        // 提取纳秒余数部分
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tp - secs).count();

        std::time_t t = std::chrono::system_clock::to_time_t(secs);
        std::tm tm_buf;

        // 跨平台线程安全的本地时间转换
#if defined(_WIN32)
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
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
    inline TimePoint parseTime(const std::string& time_str) {
        std::tm tm_buf = {};
        int ns = 0;

        std::istringstream ss(time_str);

        int d = 0, m = 0, y = 0, h = 0, min = 0, s = 0;
        char sep1, sep2, sep3, sep4, sep5;

        // 1. 依次提取数字和分隔符：日 / 月 / 年 , 时 : 分 : 秒
        if (ss >> d >> sep1 >> m >> sep2 >> y >> sep3 >> h >> sep4 >> min >> sep5 >> s) {
            // 验证提取到的分隔符是否严格符合 COMTRADE 规范
            if (sep1 == '/' && sep2 == '/' && sep3 == ',' && sep4 == ':' && sep5 == ':') {
                tm_buf.tm_mday = d;
                tm_buf.tm_mon = m - 1;      // tm_mon 范围是 0-11
                tm_buf.tm_year = y - 1900;  // tm_year 是自 1900 年起的年数
                tm_buf.tm_hour = h;
                tm_buf.tm_min = min;
                tm_buf.tm_sec = s;
            }
        }

        // 2. 尝试解析小数部分（纳秒）
        char dot;
        if (ss >> dot && dot == '.') {
            std::string frac_str;
            ss >> frac_str;

            // 确保只保留数字部分
            frac_str.erase(std::remove_if(frac_str.begin(), frac_str.end(),
                           [](unsigned char c) { return !std::isdigit(c); }),
                           frac_str.end());

            if (!frac_str.empty()) {
                // 截断或补齐至 9 位以对齐纳秒 (例如: "15" -> "150000000")
                if (frac_str.length() > 9) frac_str = frac_str.substr(0, 9);
                while (frac_str.length() < 9) frac_str += '0';

                try {
                    ns = std::stoi(frac_str);
                } catch (const std::exception&) {
                    ns = 0; // 转换溢出或失败时的安全后备
                }
            }
        }

        const std::time_t t = std::mktime(&tm_buf);
        const auto tp = std::chrono::system_clock::from_time_t(t);
        return std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) + std::chrono::nanoseconds(ns);
    }
}

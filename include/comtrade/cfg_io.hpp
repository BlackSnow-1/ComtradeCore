/**
 * @file cfg_io.hpp
 * @brief 按 IEEE/IEC C37.111 字段顺序解析 CFG，并兼容部分厂商扩展。
 */
#pragma once

#include "text_encoding.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <type_traits>
#include <vector>

namespace comtrade::detail {

// Numeric fields consume the entire trimmed token; never accept a numeric prefix.
template<class T>
inline T cfgNumber(const std::string& text) {
    const auto fields = utils::split(text);
    if (fields.size() != 1 || fields[0].empty()) throw std::invalid_argument("Empty CFG number");
    const auto& token = fields[0];
    size_t used = 0;
    if constexpr (std::is_integral_v<T>) {
        const auto number = std::stoll(token, &used);
        if (used != token.size() || number < std::numeric_limits<T>::lowest() ||
            number > std::numeric_limits<T>::max()) throw std::invalid_argument("Invalid CFG integer");
        return static_cast<T>(number);
    } else {
        const auto number = std::stod(token, &used);
        if (used != token.size() || !std::isfinite(number)) throw std::invalid_argument("Invalid CFG number");
        return static_cast<T>(number);
    }
}

inline int parseChannelCount(const std::string& token, char suffix, Warnings& warnings) {
    auto number = token;
    if (!number.empty() && (number.back() == suffix || number.back() == static_cast<char>(std::tolower(suffix)))) {
        if (number.back() != suffix) addWarning(warnings, "cfg.channel_suffix", "Lowercase channel-count suffix accepted", 2);
        number.pop_back();
    } else {
        // A missing suffix is unambiguous only if the complete field is an integer.
        addWarning(warnings, "cfg.channel_suffix", "Missing channel-count suffix accepted", 2);
    }
    const auto count = cfgNumber<int>(number);
    if (count < 0) throw std::invalid_argument("Negative channel count");
    return count;
}

// 采样段必须完整匹配 "samples_per_second,end_sample"；完整匹配可以将其与日期行可靠区分。
inline bool tryParseSampleRate(const std::string& line, SampleRate& sample_rate) {
    const auto tokens = utils::split(line);
    if (tokens.size() < 2) return false;

    try {
        std::size_t rate_characters = 0;
        std::size_t end_characters = 0;
        const double rate = std::stod(tokens[0], &rate_characters);
        const auto end_sample = std::stoull(tokens[1], &end_characters);
        if (rate_characters != tokens[0].size() || end_characters != tokens[1].size() ||
            !std::isfinite(rate) || rate < 0.0 || end_sample > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        sample_rate = {rate, static_cast<uint32_t>(end_sample)};
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Optional metadata is recoverable; invalid conversion is never silently accepted.
template<class T>
inline void optionalNumber(const std::vector<std::string>& fields, size_t index, T& value,
                           Warnings& warnings, uint64_t row) {
    if (index >= fields.size() || fields[index].empty()) {
        addWarning(warnings, "cfg.optional_default", "Missing optional channel number; using default", row); return;
    }
    try {
        value = cfgNumber<T>(fields[index]);
    } catch (const std::exception&) {
        addWarning(warnings, "cfg.optional_default", "Invalid optional channel number; using default", row);
    }
}

// 使用局部 parsed_cfg 进行事务式解析：只有所有必需字段都合法时才覆盖输出 cfg。
inline bool parseCfgLines(const std::vector<std::string>& lines, CfgData& cfg) {
    if (lines.size() < 2) return false;

    CfgData parsed_cfg;
    auto extraFields = [&](const std::vector<std::string>& fields, size_t expected, uint64_t row) {
        if (fields.size() > expected)
            addWarning(parsed_cfg.warnings, "cfg.extra_columns", "Extra CFG fields ignored", row);
    };
    const auto identity = utils::split(lines[0]);
    const auto counts = utils::split(lines[1]);
    if (identity.size() < 2 || counts.size() < 3) return false;

    extraFields(identity, 3, 1); extraFields(counts, 3, 2);
    parsed_cfg.station_name = identity[0];
    parsed_cfg.rec_dev_id = identity[1];
    if (identity.size() > 2) parsed_cfg.version = utils::determineVersion(identity[2]);

    try { parsed_cfg.total_channels = cfgNumber<int>(counts[0]); }
    catch (const std::exception&) {
        parsed_cfg.total_channels = -1;
        addWarning(parsed_cfg.warnings, "cfg.total_default", "Invalid total channel count; using A/D counts", 2);
    }
    parsed_cfg.analog_count = parseChannelCount(counts[1], 'A', parsed_cfg.warnings);
    parsed_cfg.digital_count = parseChannelCount(counts[2], 'D', parsed_cfg.warnings);
    if (parsed_cfg.analog_count < 0 || parsed_cfg.digital_count < 0 ||
        static_cast<uint64_t>(parsed_cfg.analog_count) + parsed_cfg.digital_count > lines.size()) return false;
    if (parsed_cfg.total_channels != parsed_cfg.analog_count + parsed_cfg.digital_count) {
        addWarning(parsed_cfg.warnings, "cfg.channel_count", "Total channel count corrected from A/D counts", 2);
        parsed_cfg.total_channels = parsed_cfg.analog_count + parsed_cfg.digital_count;
    }
    if (identity.size() > 2 && identity[2] != "1991" && identity[2] != "1999" && identity[2] != "2013")
        addWarning(parsed_cfg.warnings, "cfg.version", "Unknown revision interpreted using legacy layout", 1);

    // CFG 的后续字段依赖前面声明的通道数，因此使用游标严格顺序消费各行。
    std::size_t cursor = 2;
    parsed_cfg.analog_channels.reserve(static_cast<std::size_t>(parsed_cfg.analog_count));
    for (int i = 0; i < parsed_cfg.analog_count; ++i, ++cursor) {
        if (cursor >= lines.size()) return false;
        const auto tokens = utils::split(lines[cursor]);
        if (tokens.size() < 7) return false;

        extraFields(tokens, 13, cursor+1);
        AnalogChannel channel;
        channel.index = i + 1;
        optionalNumber(tokens, 0, channel.index, parsed_cfg.warnings, cursor+1);
        channel.id = tokens[1];
        if (tokens.size() > 2) channel.phase = tokens[2];
        if (tokens.size() > 3) channel.ccbm = tokens[3];
        if (tokens.size() > 4) channel.uu = tokens[4];
        channel.a = cfgNumber<double>(tokens[5]);
        channel.b = cfgNumber<double>(tokens[6]);
        optionalNumber(tokens, 7, channel.skew, parsed_cfg.warnings, cursor+1);
        optionalNumber(tokens, 8, channel.min, parsed_cfg.warnings, cursor+1);
        optionalNumber(tokens, 9, channel.max, parsed_cfg.warnings, cursor+1);
        optionalNumber(tokens, 10, channel.primary, parsed_cfg.warnings, cursor+1);
        optionalNumber(tokens, 11, channel.secondary, parsed_cfg.warnings, cursor+1);
        if (tokens.size() > 12) channel.ps = tokens[12];
        parsed_cfg.analog_channels.push_back(std::move(channel));
    }

    parsed_cfg.digital_channels.reserve(static_cast<std::size_t>(parsed_cfg.digital_count));
    for (int i = 0; i < parsed_cfg.digital_count; ++i, ++cursor) {
        if (cursor >= lines.size()) return false;
        const auto tokens = utils::split(lines[cursor]);
        if (tokens.size() < 2) return false;

        extraFields(tokens, 5, cursor+1);
        DigitalChannel channel;
        channel.index = i + 1;
        optionalNumber(tokens, 0, channel.index, parsed_cfg.warnings, cursor+1);
        channel.id = tokens[1];
        if (tokens.size() > 2) channel.phase = tokens[2];
        if (tokens.size() > 3) channel.ccbm = tokens[3];
        optionalNumber(tokens, 4, channel.normal_state, parsed_cfg.warnings, cursor+1);
        parsed_cfg.digital_channels.push_back(std::move(channel));
    }

    if (cursor >= lines.size()) return false;
    try {
        const double frequency = cfgNumber<double>(lines[cursor]);
        if (frequency <= 0) throw std::invalid_argument("frequency");
        parsed_cfg.line_frequency = frequency;
    } catch (...) { addWarning(parsed_cfg.warnings, "cfg.frequency", "Invalid nominal frequency; using 50 Hz", cursor+1); }
    ++cursor;

    if (cursor >= lines.size()) return false;
    const int sample_rate_count = cfgNumber<int>(lines[cursor++]);
    if (sample_rate_count < 0 || cursor + static_cast<std::size_t>(sample_rate_count) + 3 > lines.size()) {
        return false;
    }
    parsed_cfg.variable_sample_rate = sample_rate_count == 0;

    parsed_cfg.sample_rates.reserve(static_cast<std::size_t>(sample_rate_count));
    for (int i = 0; i < sample_rate_count; ++i, ++cursor) {
        SampleRate sample_rate;
        if (!tryParseSampleRate(lines[cursor], sample_rate)) return false;
        extraFields(utils::split(lines[cursor]), 2, cursor+1);
        parsed_cfg.sample_rates.push_back(sample_rate);
    }

    // nrates=0 表示采样率段数量不预先声明；持续读取合法采样段，直到遇到开始时间行。
    // 同一逻辑也兼容使用该布局的早期厂商文件。
    while (sample_rate_count == 0 && cursor < lines.size()) {
        SampleRate sample_rate;
        if (!tryParseSampleRate(lines[cursor], sample_rate)) break;
        extraFields(utils::split(lines[cursor]), 2, cursor+1);
        parsed_cfg.sample_rates.push_back(sample_rate);
        ++cursor;
    }

    if (cursor + 3 > lines.size()) return false;

    const auto& start_time_text = lines[cursor++];
    const auto& trigger_time_text = lines[cursor++];

    parsed_cfg.timestamp_fractional_digits = std::max(
        utils::fractionalSecondDigits(start_time_text),
        utils::fractionalSecondDigits(trigger_time_text));

    const auto data_type_tokens = utils::split(lines[cursor++]);
    if (data_type_tokens.empty()) return false;
    extraFields(data_type_tokens, 1, cursor);
    parsed_cfg.data_type = DataTypeUtils::FromString(data_type_tokens[0]);

    if (parsed_cfg.version != StandardVersion::V1991) {
        if (cursor >= lines.size())
            addWarning(parsed_cfg.warnings, "cfg.timemult", "Missing TIMEMULT; using 1");
        else {
            const auto fields = utils::split(lines[cursor]);
            if (fields.size() == 1) {
                try { parsed_cfg.time_multiplier = cfgNumber<double>(lines[cursor]); }
                catch (...) { addWarning(parsed_cfg.warnings, "cfg.timemult", "Invalid TIMEMULT; using 1"); }
                ++cursor;
            } else addWarning(parsed_cfg.warnings, "cfg.timemult", "Missing TIMEMULT; using 1");
        }
    } else if (cursor < lines.size()) {
        // A few pre-1999 vendor revisions (for example, "1997") already
        // include the later time-multiplier field. Preserve it when present.
        const auto tokens = utils::split(lines[cursor]);
        if (tokens.size() == 1 && !tokens[0].empty()) {
            try {
                std::size_t parsed_characters = 0;
                const double multiplier = std::stod(tokens[0], &parsed_characters);
                if (parsed_characters == tokens[0].size()) {
                    parsed_cfg.time_multiplier = multiplier;
                    ++cursor;
                }
            } catch (const std::exception&) {
                // A non-numeric trailing extension is unrelated to TIMEMULT.
            }
        }
    }
    if (!std::isfinite(parsed_cfg.time_multiplier) || parsed_cfg.time_multiplier <= 0.0) {
        parsed_cfg.time_multiplier = 1;
        addWarning(parsed_cfg.warnings, "cfg.timemult", "Invalid TIMEMULT; using 1");
    }

    if (parsed_cfg.version == StandardVersion::V2013) {
        parsed_cfg.time_code = "x"; parsed_cfg.local_code = "x";
        if (cursor < lines.size()) {
            const auto codes = utils::split(lines[cursor++]);
            extraFields(codes, 2, cursor);
            if (!codes.empty()) parsed_cfg.time_code = codes[0];
            if (codes.size() > 1) parsed_cfg.local_code = codes[1];
        } else addWarning(parsed_cfg.warnings, "cfg.time_metadata", "Missing 2013 time codes");
        if (cursor < lines.size()) {
            const auto quality = utils::split(lines[cursor++]);
            extraFields(quality, 2, cursor);
            if (!quality.empty()) parsed_cfg.time_quality_code = quality[0];
            try { if (quality.size() > 1) parsed_cfg.leap_second = cfgNumber<int>(quality[1]); }
            catch (...) { addWarning(parsed_cfg.warnings, "cfg.leap_second", "Invalid leap second indicator; using 0"); }
        } else addWarning(parsed_cfg.warnings, "cfg.time_metadata", "Missing 2013 time quality");
        if (parsed_cfg.leap_second < 0 || parsed_cfg.leap_second > 3) {
            parsed_cfg.leap_second = 0;
            addWarning(parsed_cfg.warnings, "cfg.leap_second", "Invalid leap second indicator; using 0");
        }
        int ignored_offset = 0;
        if (parsed_cfg.local_code != "x" && !utils::parseUtcOffset(parsed_cfg.local_code, ignored_offset))
            addWarning(parsed_cfg.warnings, "cfg.local_code", "Unrecognized local time code preserved");
        parsed_cfg.utc_offset_known = utils::parseUtcOffset(parsed_cfg.time_code, parsed_cfg.utc_offset_minutes);
        if (parsed_cfg.time_quality_code.size() != 1 ||
            std::string("0123456789ABCDEFabcdef").find(parsed_cfg.time_quality_code) == std::string::npos)
            addWarning(parsed_cfg.warnings, "cfg.time_quality", "Unrecognized time quality code");
        if (parsed_cfg.time_quality_code == "F" || parsed_cfg.time_quality_code == "f")
            addWarning(parsed_cfg.warnings, "cfg.clock", "Clock failure: absolute time is unreliable");
        if (parsed_cfg.leap_second)
            addWarning(parsed_cfg.warnings, "cfg.leap_second", "Leap second metadata present; no automatic correction applied");
    }
    if (!parsed_cfg.utc_offset_known)
        addWarning(parsed_cfg.warnings, "cfg.timezone_unknown", "UTC offset unavailable; legacy machine-local interpretation retained");
    // Dates cannot be fabricated: unlike optional metadata, an invalid date is fatal.
    parsed_cfg.start_time = utils::parseTime(start_time_text, parsed_cfg.utc_offset_known, parsed_cfg.utc_offset_minutes);
    parsed_cfg.trigger_time = utils::parseTime(trigger_time_text, parsed_cfg.utc_offset_known, parsed_cfg.utc_offset_minutes);
    uint32_t previous_end = 0;
    for (const auto& rate : parsed_cfg.sample_rates) {
        if (rate.end_sample <= previous_end || rate.samples_per_second <= 0)
            addWarning(parsed_cfg.warnings, "cfg.sample_rates", "Non-positive rate or non-increasing segment boundary; reconstruction may be unavailable");
        previous_end = rate.end_sample;
    }
    for (size_t i = 0; i < parsed_cfg.analog_channels.size(); ++i) {
        const auto& c = parsed_cfg.analog_channels[i];
        if (c.index != static_cast<int>(i+1)) addWarning(parsed_cfg.warnings, "cfg.channel_index", "Nonsequential analog channel ID");
        if (!std::isfinite(c.a) || !std::isfinite(c.b)) return false;
        if (!c.a || c.min > c.max || c.secondary <= 0 || (c.ps != "P" && c.ps != "S"))
            addWarning(parsed_cfg.warnings, "cfg.analog_metadata", "Suspicious analog scale/range/ratio metadata preserved");
    }
    for (size_t i = 0; i < parsed_cfg.digital_channels.size(); ++i) {
        const auto& c = parsed_cfg.digital_channels[i];
        if (c.index != static_cast<int>(i+1)) addWarning(parsed_cfg.warnings, "cfg.channel_index", "Nonsequential digital channel ID", i+3+parsed_cfg.analog_count);
        if (c.normal_state != 0 && c.normal_state != 1)
            addWarning(parsed_cfg.warnings, "cfg.digital_state", "Nonboolean normal state preserved");
    }
    if (cursor < lines.size()) addWarning(parsed_cfg.warnings, "cfg.trailing", "Trailing CFG extension preserved only in source file");

    cfg = std::move(parsed_cfg);
    return true;
}

// 文件层只负责读取与编码规范化；格式异常统一转换为 false，避免数值转换异常泄漏到调用方。
inline bool parseCfgFile(const std::string& cfg_filepath, CfgData& cfg) {
    std::ifstream cfg_file(cfg_filepath);
    if (!cfg_file.is_open()) return false;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(cfg_file, line)) {
        lines.push_back(std::move(line));
    }

    // 解析前统一为 UTF-8，使站名和通道名在 Linux/Windows 上具有一致语义。
    normalizeCfgLinesToUtf8(lines);

    try {
        return parseCfgLines(lines, cfg);
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace comtrade::detail

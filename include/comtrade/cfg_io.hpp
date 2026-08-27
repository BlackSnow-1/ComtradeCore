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
#include <vector>

namespace comtrade::detail {

// 通道计数字段形如 "12A" 或 "29D"，数值部分位于类型后缀之前。
inline int parseChannelCount(const std::string& token, char suffix) {
    const auto suffix_position = token.find(suffix);
    return std::stoi(token.substr(0, suffix_position));
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

// 使用局部 parsed_cfg 进行事务式解析：只有所有必需字段都合法时才覆盖输出 cfg。
inline bool parseCfgLines(const std::vector<std::string>& lines, CfgData& cfg) {
    if (lines.size() < 2) return false;

    CfgData parsed_cfg;
    const auto identity = utils::split(lines[0]);
    const auto counts = utils::split(lines[1]);
    if (identity.size() < 2 || counts.size() < 3) return false;

    parsed_cfg.station_name = identity[0];
    parsed_cfg.rec_dev_id = identity[1];
    if (identity.size() > 2) parsed_cfg.version = utils::determineVersion(identity[2]);

    parsed_cfg.total_channels = std::stoi(counts[0]);
    parsed_cfg.analog_count = parseChannelCount(counts[1], 'A');
    parsed_cfg.digital_count = parseChannelCount(counts[2], 'D');
    if (parsed_cfg.analog_count < 0 || parsed_cfg.digital_count < 0 ||
        parsed_cfg.total_channels != parsed_cfg.analog_count + parsed_cfg.digital_count) {
        return false;
    }

    // CFG 的后续字段依赖前面声明的通道数，因此使用游标严格顺序消费各行。
    std::size_t cursor = 2;
    parsed_cfg.analog_channels.reserve(static_cast<std::size_t>(parsed_cfg.analog_count));
    for (int i = 0; i < parsed_cfg.analog_count; ++i, ++cursor) {
        if (cursor >= lines.size()) return false;
        const auto tokens = utils::split(lines[cursor]);
        if (tokens.size() < 7) return false;

        AnalogChannel channel;
        channel.index = std::stoi(tokens[0]);
        channel.id = tokens[1];
        if (tokens.size() > 2) channel.phase = tokens[2];
        if (tokens.size() > 3) channel.ccbm = tokens[3];
        if (tokens.size() > 4) channel.uu = tokens[4];
        channel.a = std::stod(tokens[5]);
        channel.b = std::stod(tokens[6]);
        if (tokens.size() > 7) channel.skew = std::stod(tokens[7]);
        if (tokens.size() > 8) channel.min = std::stoi(tokens[8]);
        if (tokens.size() > 9) channel.max = std::stoi(tokens[9]);
        if (tokens.size() > 10) channel.primary = std::stod(tokens[10]);
        if (tokens.size() > 11) channel.secondary = std::stod(tokens[11]);
        if (tokens.size() > 12) channel.ps = tokens[12];
        parsed_cfg.analog_channels.push_back(std::move(channel));
    }

    parsed_cfg.digital_channels.reserve(static_cast<std::size_t>(parsed_cfg.digital_count));
    for (int i = 0; i < parsed_cfg.digital_count; ++i, ++cursor) {
        if (cursor >= lines.size()) return false;
        const auto tokens = utils::split(lines[cursor]);
        if (tokens.size() < 2) return false;

        DigitalChannel channel;
        channel.index = std::stoi(tokens[0]);
        channel.id = tokens[1];
        if (tokens.size() > 2) channel.phase = tokens[2];
        if (tokens.size() > 3) channel.ccbm = tokens[3];
        if (tokens.size() > 4) channel.normal_state = std::stoi(tokens[4]);
        parsed_cfg.digital_channels.push_back(std::move(channel));
    }

    if (cursor >= lines.size()) return false;
    parsed_cfg.line_frequency = std::stod(lines[cursor++]);

    if (cursor >= lines.size()) return false;
    const int sample_rate_count = std::stoi(lines[cursor++]);
    if (sample_rate_count < 0 || cursor + static_cast<std::size_t>(sample_rate_count) + 3 > lines.size()) {
        return false;
    }
    parsed_cfg.variable_sample_rate = sample_rate_count == 0;

    parsed_cfg.sample_rates.reserve(static_cast<std::size_t>(sample_rate_count));
    for (int i = 0; i < sample_rate_count; ++i, ++cursor) {
        SampleRate sample_rate;
        if (!tryParseSampleRate(lines[cursor], sample_rate)) return false;
        parsed_cfg.sample_rates.push_back(sample_rate);
    }

    // nrates=0 表示采样率段数量不预先声明；持续读取合法采样段，直到遇到开始时间行。
    // 同一逻辑也兼容使用该布局的早期厂商文件。
    while (sample_rate_count == 0 && cursor < lines.size()) {
        SampleRate sample_rate;
        if (!tryParseSampleRate(lines[cursor], sample_rate)) break;
        parsed_cfg.sample_rates.push_back(sample_rate);
        ++cursor;
    }

    if (cursor + 3 > lines.size()) return false;

    const auto& start_time_text = lines[cursor++];
    const auto& trigger_time_text = lines[cursor++];
    parsed_cfg.start_time = utils::parseTime(start_time_text);
    parsed_cfg.trigger_time = utils::parseTime(trigger_time_text);
    parsed_cfg.timestamp_fractional_digits = std::max(
        utils::fractionalSecondDigits(start_time_text),
        utils::fractionalSecondDigits(trigger_time_text));

    const auto data_type_tokens = utils::split(lines[cursor++]);
    if (data_type_tokens.empty()) return false;
    parsed_cfg.data_type = DataTypeUtils::FromString(data_type_tokens[0]);

    if (parsed_cfg.version != StandardVersion::V1991) {
        if (cursor >= lines.size()) return false;
        parsed_cfg.time_multiplier = std::stod(lines[cursor++]);
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
    if (!std::isfinite(parsed_cfg.time_multiplier) || parsed_cfg.time_multiplier <= 0.0) return false;

    // 2013 版追加时间码、时区、质量码和闰秒状态；旧版本没有这两行。
    if (parsed_cfg.version == StandardVersion::V2013) {
        if (cursor + 2 > lines.size()) return false;

        const auto time_codes = utils::split(lines[cursor++]);
        const auto time_quality = utils::split(lines[cursor++]);
        if (time_codes.size() < 2 || time_quality.size() < 2 || time_quality[0].empty()) return false;

        parsed_cfg.time_code = time_codes[0];
        parsed_cfg.local_code = time_codes[1];
        parsed_cfg.time_quality_code = time_quality[0];
        parsed_cfg.leap_second = std::stoi(time_quality[1]);
        // IEEE/IEC C37.111-2013 defines four leap-second indicators:
        // 0=no adjustment, 1=added, 2=subtracted, 3=source cannot handle it.
        if (parsed_cfg.leap_second < 0 || parsed_cfg.leap_second > 3) return false;
    }

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

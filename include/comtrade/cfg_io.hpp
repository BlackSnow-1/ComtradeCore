#pragma once

#include "types.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace comtrade::detail {

inline int parseChannelCount(const std::string& token, char suffix) {
    const auto suffix_position = token.find(suffix);
    return std::stoi(token.substr(0, suffix_position));
}

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

    parsed_cfg.sample_rates.reserve(static_cast<std::size_t>(sample_rate_count));
    for (int i = 0; i < sample_rate_count; ++i, ++cursor) {
        const auto tokens = utils::split(lines[cursor]);
        if (tokens.size() < 2) return false;
        parsed_cfg.sample_rates.push_back({std::stod(tokens[0]), static_cast<uint32_t>(std::stoul(tokens[1]))});
    }

    // Some recorders use nrates=0 for a variable or unspecified sample rate,
    // but still emit one "0,end_sample" line. Accept that vendor extension
    // without confusing the following comma-separated start time for a rate.
    if (sample_rate_count == 0 && cursor < lines.size()) {
        const auto tokens = utils::split(lines[cursor]);
        if (tokens.size() >= 2 && tokens[0].find_first_of("/:") == std::string::npos &&
            tokens[1].find_first_of("/:") == std::string::npos) {
            parsed_cfg.sample_rates.push_back(
                {std::stod(tokens[0]), static_cast<uint32_t>(std::stoul(tokens[1]))});
            ++cursor;
        }
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

    if (parsed_cfg.version == StandardVersion::V2013) {
        if (cursor + 2 > lines.size()) return false;

        const auto time_codes = utils::split(lines[cursor++]);
        const auto time_quality = utils::split(lines[cursor++]);
        if (time_codes.size() < 2 || time_quality.size() < 2 || time_quality[0].empty()) return false;

        parsed_cfg.time_code = time_codes[0];
        parsed_cfg.local_code = time_codes[1];
        parsed_cfg.time_quality_code = time_quality[0];
        parsed_cfg.leap_second = std::stoi(time_quality[1]);
        if (parsed_cfg.leap_second < -1 || parsed_cfg.leap_second > 1) return false;
    }

    cfg = std::move(parsed_cfg);
    return true;
}

inline bool parseCfgFile(const std::string& cfg_filepath, CfgData& cfg) {
    std::ifstream cfg_file(cfg_filepath);
    if (!cfg_file.is_open()) return false;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(cfg_file, line)) {
        lines.push_back(std::move(line));
    }

    try {
        return parseCfgLines(lines, cfg);
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace comtrade::detail

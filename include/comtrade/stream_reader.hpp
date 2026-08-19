#pragma once

#include "types.hpp"
#include "utils.hpp"

#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace comtrade {

struct SampleRow {
    uint32_t index = 0;
    uint32_t timestamp_us = 0;
    std::vector<double> analog_values;
    std::vector<bool> digital_values;
};

class StreamReader {
public:
    explicit StreamReader(const std::string& cfg_filepath) {
        if (!loadCfg(cfg_filepath)) {
            throw std::runtime_error("Failed to load COMTRADE CFG file: " + cfg_filepath);
        }
    }

    [[nodiscard]] const CfgData& getCfg() const noexcept {
        return cfg_;
    }

    size_t processDatStream(const std::string& dat_filepath,
                            const std::function<void(const SampleRow&)>& on_row_parsed) const {
        if (cfg_.data_type != DataType::ASCII) return 0;

        std::ifstream dat_file(dat_filepath);
        if (!dat_file.is_open()) return 0;

        SampleRow row_buffer;
        row_buffer.analog_values.resize(static_cast<std::size_t>(cfg_.analog_count));
        row_buffer.digital_values.resize(static_cast<std::size_t>(cfg_.digital_count));

        size_t parsed_count = 0;
        std::string line;
        while (std::getline(dat_file, line)) {
            const auto tokens = utils::split(line);
            const auto expected_tokens = static_cast<std::size_t>(2 + cfg_.analog_count + cfg_.digital_count);
            if (tokens.size() < expected_tokens) continue;

            try {
                row_buffer.index = static_cast<uint32_t>(std::stoul(tokens[0]));
                row_buffer.timestamp_us = static_cast<uint32_t>(std::stoul(tokens[1]));

                for (int i = 0; i < cfg_.analog_count; ++i) {
                    const double raw_value = std::stod(tokens[static_cast<std::size_t>(2 + i)]);
                    const auto& channel = cfg_.analog_channels[static_cast<std::size_t>(i)];
                    row_buffer.analog_values[static_cast<std::size_t>(i)] = raw_value * channel.a + channel.b;
                }

                for (int i = 0; i < cfg_.digital_count; ++i) {
                    const auto token_index = static_cast<std::size_t>(2 + cfg_.analog_count + i);
                    row_buffer.digital_values[static_cast<std::size_t>(i)] = std::stoul(tokens[token_index]) != 0;
                }
            } catch (const std::exception&) {
                continue;
            }

            on_row_parsed(row_buffer);
            ++parsed_count;
        }

        return parsed_count;
    }

private:
    [[nodiscard]] bool loadCfg(const std::string& cfg_filepath) {
        std::ifstream cfg_file(cfg_filepath);
        if (!cfg_file.is_open()) return false;

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(cfg_file, line)) {
            lines.push_back(std::move(line));
        }

        try {
            return parseCfgLines(lines);
        } catch (const std::exception&) {
            return false;
        }
    }

    static int parseChannelCount(const std::string& token, char suffix) {
        const auto suffix_position = token.find(suffix);
        return std::stoi(token.substr(0, suffix_position));
    }

    bool parseCfgLines(const std::vector<std::string>& lines) {
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
        cursor += static_cast<std::size_t>(sample_rate_count);

        parsed_cfg.start_time = utils::parseTime(lines[cursor++]);
        parsed_cfg.trigger_time = utils::parseTime(lines[cursor++]);
        const auto data_type_tokens = utils::split(lines[cursor++]);
        if (data_type_tokens.empty()) return false;
        parsed_cfg.data_type = DataTypeUtils::FromString(data_type_tokens[0]);

        cfg_ = std::move(parsed_cfg);
        return true;
    }

    CfgData cfg_;
};

}  // namespace comtrade

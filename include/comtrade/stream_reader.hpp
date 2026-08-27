/**
 * @file stream_reader.hpp
 * @brief 以恒定内存逐行解析 ASCII DAT，并结合 CFG 输出工程量和绝对时间。
 */
#pragma once

#include "cfg_io.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace comtrade {

// 回调看到的单个采样点。raw_timestamp 保留 DAT 原值，其余时间字段为换算结果。
struct SampleRow {
    uint32_t index = 0;
    uint32_t raw_timestamp = 0;
    uint64_t timestamp_us = 0;
    std::chrono::nanoseconds time_offset{};
    TimePoint absolute_time{};
    std::vector<double> analog_values;
    std::vector<bool> digital_values;
};

// StreamReader 在构造时固定一份 CFG 快照，之后可用它重复流式处理同格式 DAT。
class StreamReader {
public:
    explicit StreamReader(const std::string& cfg_filepath) {
        if (!detail::parseCfgFile(cfg_filepath, cfg_)) {
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

        // 整个文件复用同一个行缓冲，回调不得在返回后继续持有 row 或内部容器的引用。
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
                row_buffer.raw_timestamp = static_cast<uint32_t>(std::stoul(tokens[1]));

                // TIMEMULT 表示每个原始时间单位对应的微秒数；先转纳秒可保留小数微秒。
                const long double offset_ns = static_cast<long double>(row_buffer.raw_timestamp) *
                                              static_cast<long double>(cfg_.time_multiplier) * 1000.0L;
                if (offset_ns > static_cast<long double>(std::numeric_limits<int64_t>::max())) continue;
                row_buffer.time_offset = std::chrono::nanoseconds(static_cast<int64_t>(std::llround(offset_ns)));
                row_buffer.timestamp_us = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(row_buffer.time_offset).count());
                row_buffer.absolute_time = cfg_.start_time + row_buffer.time_offset;

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
                // 单行损坏不终止整份录波；只有成功解析的行才触发回调并计数。
                continue;
            }

            on_row_parsed(row_buffer);
            ++parsed_count;
        }

        return parsed_count;
    }

private:
    CfgData cfg_;
};

}  // namespace comtrade

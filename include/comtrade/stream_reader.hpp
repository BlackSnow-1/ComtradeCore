/**
 * @file stream_reader.hpp
 * @brief 以恒定内存逐行解析 ASCII 和二进制 DAT，并结合 CFG 输出工程量和绝对时间。
 */
#pragma once

#include "binary_io.hpp"
#include "cfg_io.hpp"
#include "text_encoding.hpp"
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
    // ASCII DAT files may use values beyond the 32-bit binary timestamp field.
    uint64_t raw_timestamp = 0;
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
        if (!std::isfinite(cfg_.time_multiplier) || cfg_.time_multiplier <= 0.0 ||
            cfg_.analog_count < 0 || cfg_.digital_count < 0 ||
            cfg_.analog_channels.size() != static_cast<std::size_t>(cfg_.analog_count) ||
            cfg_.digital_channels.size() != static_cast<std::size_t>(cfg_.digital_count)) {
            return 0;
        }

        std::ifstream dat_file(dat_filepath, std::ios::binary);
        if (!dat_file.is_open()) return 0;

        // 整个文件复用同一个行缓冲，回调不得在返回后继续持有 row 或内部容器的引用。
        SampleRow row_buffer;
        row_buffer.analog_values.resize(static_cast<std::size_t>(cfg_.analog_count));
        row_buffer.digital_values.resize(static_cast<std::size_t>(cfg_.digital_count));

        if (cfg_.data_type == DataType::ASCII) {
            return processAsciiDat(dat_file, row_buffer, on_row_parsed);
        }
        if (detail::isBinaryDataType(cfg_.data_type)) {
            return processBinaryDat(dat_file, row_buffer, on_row_parsed);
        }
        return 0;
    }

private:
    bool populateTimeFields(SampleRow& row) const {
        // TIMEMULT 表示每个原始时间单位对应的微秒数；先转纳秒可保留小数微秒。
        const long double offset_ns = static_cast<long double>(row.raw_timestamp) *
                                      static_cast<long double>(cfg_.time_multiplier) * 1000.0L;
        if (offset_ns < 0.0L ||
            offset_ns > static_cast<long double>(std::numeric_limits<int64_t>::max())) {
            return false;
        }

        row.time_offset = std::chrono::nanoseconds(static_cast<int64_t>(std::llround(offset_ns)));
        row.timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(row.time_offset).count());
        row.absolute_time = cfg_.start_time + row.time_offset;
        return true;
    }

    size_t processAsciiDat(std::istream& dat_file,
                           SampleRow& row_buffer,
                           const std::function<void(const SampleRow&)>& on_row_parsed) const {
        size_t parsed_count = 0;
        std::string line;
        bool first_line = true;
        while (std::getline(dat_file, line)) {
            if (first_line) {
                detail::stripUtf8Bom(line);
                first_line = false;
            }
            const auto tokens = utils::split(line);
            const auto expected_tokens = static_cast<std::size_t>(2 + cfg_.analog_count + cfg_.digital_count);
            if (tokens.size() < expected_tokens) continue;

            try {
                std::size_t index_characters = 0;
                std::size_t timestamp_characters = 0;
                const auto parsed_index = std::stoull(tokens[0], &index_characters);
                const auto parsed_timestamp = std::stoull(tokens[1], &timestamp_characters);
                if (tokens[0].empty() || tokens[1].empty() || tokens[0].front() == '-' || tokens[1].front() == '-' ||
                    index_characters != tokens[0].size() || timestamp_characters != tokens[1].size() ||
                    parsed_index > std::numeric_limits<uint32_t>::max()) {
                    continue;
                }
                row_buffer.index = static_cast<uint32_t>(parsed_index);
                row_buffer.raw_timestamp = static_cast<uint64_t>(parsed_timestamp);
                if (!populateTimeFields(row_buffer)) continue;

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

    size_t processBinaryDat(std::istream& dat_file,
                            SampleRow& row_buffer,
                            const std::function<void(const SampleRow&)>& on_row_parsed) const {
        const auto analog_count = static_cast<std::size_t>(cfg_.analog_count);
        const auto digital_count = static_cast<std::size_t>(cfg_.digital_count);
        const auto row_size = detail::binaryRowSize(cfg_.data_type, analog_count, digital_count);
        std::vector<char> encoded_row(row_size);

        size_t parsed_count = 0;
        while (dat_file.read(encoded_row.data(), static_cast<std::streamsize>(encoded_row.size()))) {
            const char* cursor = encoded_row.data();
            row_buffer.index = detail::readUint32LittleEndian(cursor);
            row_buffer.raw_timestamp = detail::readUint32LittleEndian(cursor);
            if (!populateTimeFields(row_buffer)) continue;

            // BINARY、BINARY32 和 FLOAT32 仅改变模拟量原始字段宽度；工程量换算规则一致。
            for (std::size_t i = 0; i < analog_count; ++i) {
                double raw_value = 0.0;
                if (cfg_.data_type == DataType::BINARY) {
                    raw_value = static_cast<double>(detail::readInt16LittleEndian(cursor));
                } else if (cfg_.data_type == DataType::BINARY32) {
                    raw_value = static_cast<double>(detail::readInt32LittleEndian(cursor));
                } else {
                    raw_value = static_cast<double>(detail::readFloat32LittleEndian(cursor));
                }

                const auto& channel = cfg_.analog_channels[i];
                row_buffer.analog_values[i] = raw_value * channel.a + channel.b;
            }

            // 数字量按每 16 路一个 uint16 word 打包，通道 0 对应最低有效位。
            for (std::size_t word_index = 0;
                 word_index < detail::digitalWordCount(digital_count);
                 ++word_index) {
                const auto word = detail::readUint16LittleEndian(cursor);
                for (std::size_t bit = 0; bit < 16U; ++bit) {
                    const auto channel_index = word_index * 16U + bit;
                    if (channel_index >= digital_count) break;
                    row_buffer.digital_values[channel_index] = (word & (std::uint16_t{1} << bit)) != 0U;
                }
            }

            on_row_parsed(row_buffer);
            ++parsed_count;
        }

        // read() 只有取得完整固定长度行才进入循环，因此文件末尾的残缺行会被安全忽略。
        return parsed_count;
    }

    CfgData cfg_;
};

}  // namespace comtrade

/**
 * @file stream_writer.hpp
 * @brief 顺序写入 ASCII/BINARY/BINARY32/FLOAT32 DAT，避免保存整份波形。
 */
#pragma once
#include "types.hpp"
#include <fstream>
#include <utility>
#include <vector>
#include <cmath>
#include <cstring> // for std::memcpy
#include <limits>
#include <stdexcept>

namespace comtrade {
    // 构造时复制 CFG，调用方后续修改原 CFG 不会改变当前输出流的列布局。
    class StreamWriter {
    private:
        CfgData cfg_;
        std::ofstream dat_file_;
        uint32_t current_row_index_ = 1;

        // 二进制模式专属：预分配的单行缓冲，pushRow 热路径不再申请内存。
        std::vector<char> row_buffer_;
        size_t digital_word_count_ = 0;

    public:
        explicit StreamWriter(CfgData cfg) : cfg_(std::move(cfg)) {
        }

        ~StreamWriter() {
            close();
        }

        bool open(const std::string &dat_filepath) {
            // Use binary mode for every DAT type so ASCII line endings are
            // emitted exactly as COMTRADE CRLF on every platform.
            std::ios_base::openmode mode = std::ios::out | std::ios::binary;

            if (cfg_.data_type == DataType::BINARY || cfg_.data_type == DataType::BINARY32 ||
                cfg_.data_type == DataType::FLOAT32) {
                // 预计算数字量需要多少个 16-bit word (向上取整)
                digital_word_count_ = (cfg_.digital_channels.size() + 15) / 16;

                // 计算单行数据精确的字节总数，并预分配缓冲区
                size_t row_bytes = 4 + 4; // Index (4) + Timestamp (4)

                if (cfg_.data_type == DataType::BINARY) {
                    row_bytes += cfg_.analog_channels.size() * 2; // 每通道 2 字节
                } else {
                    row_bytes += cfg_.analog_channels.size() * 4; // 每通道 4 字节
                }

                row_bytes += digital_word_count_ * 2; // 每个数字量 Word 占 2 字节

                row_buffer_.resize(row_bytes); // 整个生命周期只分配这一次内存
            }

            dat_file_.open(dat_filepath, mode);
            return dat_file_.is_open();
        }

        void pushRow(uint32_t timestamp_us,
                     const std::vector<double> &analog_values,
                     const std::vector<bool> &digital_values) {
            // 输入时间是相对开始时刻的微秒偏移，DAT 第二列按 TIMEMULT 反算为原始整数。
            // analog_values/digital_values 的数量必须与 CFG 声明一致。
            if (!dat_file_.is_open()) throw std::runtime_error("DAT stream not open.");
            if (!std::isfinite(cfg_.time_multiplier) || cfg_.time_multiplier <= 0.0) {
                throw std::invalid_argument("COMTRADE time multiplier must be positive and finite.");
            }

            const auto raw_timestamp = std::llround(timestamp_us / cfg_.time_multiplier);
            if (raw_timestamp < 0 || raw_timestamp > std::numeric_limits<uint32_t>::max()) {
                throw std::overflow_error("COMTRADE raw timestamp exceeds uint32 range.");
            }

            if (cfg_.data_type == DataType::ASCII) {
                writeAsciiRow(static_cast<uint32_t>(raw_timestamp), analog_values, digital_values);
            } else {
                writeBinaryRow(static_cast<uint32_t>(raw_timestamp), analog_values, digital_values);
            }

            current_row_index_++;
        }

        void close() {
            if (dat_file_.is_open()) {
                dat_file_.flush();
                dat_file_.close();
            }
        }

    private:
        void writeAsciiRow(uint32_t ts_us, const std::vector<double> &a_vals, const std::vector<bool> &d_vals) {
            dat_file_ << current_row_index_ << "," << ts_us;
            for (size_t i = 0; i < cfg_.analog_channels.size(); ++i) {
                const auto &ch = cfg_.analog_channels[i];
                // 工程值写回原始整数：raw = round((value - b) / a)。
                const auto raw_val = static_cast<int32_t>(std::round((a_vals[i] - ch.b) / ch.a));
                dat_file_ << "," << raw_val;
            }
            for (const bool val: d_vals) dat_file_ << "," << (val ? 1 : 0);
            dat_file_ << "\r\n";
        }

        // 核心性能点：单行二进制直接打包与落盘
        void writeBinaryRow(uint32_t ts_us, const std::vector<double> &a_vals, const std::vector<bool> &d_vals) {
            char *ptr = row_buffer_.data();

            // 二进制行布局固定为：序号、时间戳、模拟量数组、数字量 word 数组。
            // memcpy 避免对 char 缓冲区执行潜在未对齐的整数写入。
            std::memcpy(ptr, &current_row_index_, 4);
            ptr += 4;
            std::memcpy(ptr, &ts_us, 4);
            ptr += 4;

            // 2. 写入模拟量
            for (size_t i = 0; i < cfg_.analog_channels.size(); ++i) {
                const auto &ch = cfg_.analog_channels[i];

                if (cfg_.data_type == DataType::BINARY) {
                    auto raw_val = static_cast<int16_t>(std::round((a_vals[i] - ch.b) / ch.a));
                    std::memcpy(ptr, &raw_val, 2);
                    ptr += 2;
                } else if (cfg_.data_type == DataType::BINARY32) {
                    auto raw_val = static_cast<int32_t>(std::round((a_vals[i] - ch.b) / ch.a));
                    std::memcpy(ptr, &raw_val, 4);
                    ptr += 4;
                } else {
                    const auto raw_val = static_cast<float>((a_vals[i] - ch.b) / ch.a);
                    std::memcpy(ptr, &raw_val, 4);
                    ptr += 4;
                }
            }

            // 3. 写入数字量 (位打包)
            for (size_t w = 0; w < digital_word_count_; ++w) {
                uint16_t word = 0;
                // 遍历该 Word 对应的 16 个状态位
                for (size_t bit = 0; bit < 16; ++bit) {
                    const size_t d_idx = w * 16 + bit;
                    if (d_idx < cfg_.digital_channels.size() && d_vals[d_idx]) {
                        word |= (1 << bit); // 设置对应位为 1
                    }
                }
                std::memcpy(ptr, &word, 2);
                ptr += 2;
            }

            // 4. 单次 IO 刷入流中
            dat_file_.write(row_buffer_.data(), static_cast<std::streamsize>(row_buffer_.size()));
        }
    };
} // namespace comtrade

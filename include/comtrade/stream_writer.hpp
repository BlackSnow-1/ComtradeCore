// include/comtrade/stream_writer.hpp
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
    class StreamWriter {
    private:
        CfgData cfg_;
        std::ofstream dat_file_;
        uint32_t current_row_index_ = 1;

        // 二进制模式专属：预分配的单行二进制缓冲区
        std::vector<char> row_buffer_;
        size_t digital_word_count_ = 0;

    public:
        explicit StreamWriter(CfgData cfg) : cfg_(std::move(cfg)) {
        }

        ~StreamWriter() {
            close();
        }

        bool open(const std::string &dat_filepath) {
            std::ios_base::openmode mode = std::ios::out;

            if (cfg_.data_type == DataType::BINARY || cfg_.data_type == DataType::BINARY32 ||
                cfg_.data_type == DataType::FLOAT32) {
                mode |= std::ios::binary;

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
                const auto raw_val = static_cast<int32_t>(std::round((a_vals[i] - ch.b) / ch.a));
                dat_file_ << "," << raw_val;
            }
            for (const bool val: d_vals) dat_file_ << "," << (val ? 1 : 0);
            dat_file_ << "\n";
        }

        // 核心性能点：单行二进制直接打包与落盘
        void writeBinaryRow(uint32_t ts_us, const std::vector<double> &a_vals, const std::vector<bool> &d_vals) {
            char *ptr = row_buffer_.data();

            // 1. 写入序号和时间戳 (直接使用 memcpy 防止未对齐引起的崩溃，编译器会将其优化为高效的单条汇编指令)
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

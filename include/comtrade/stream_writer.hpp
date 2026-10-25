/**
 * @file stream_writer.hpp
 * @brief 顺序写入 ASCII/BINARY/BINARY32/FLOAT32 DAT，避免保存整份波形。
 */
#pragma once
#include "binary_io.hpp"
#include "types.hpp"
#include <fstream>
#include <utility>
#include <vector>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace comtrade {
    // 构造时复制 CFG，调用方后续修改原 CFG 不会改变当前输出流的列布局。
    class StreamWriter {
    private:
        CfgData cfg_;
        std::ofstream owned_file_;
        std::ostream* output_ = nullptr;
        std::ostream& stream() { return *output_; }
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
            close();
            owned_file_.clear();
            current_row_index_ = 1;

            // Use binary mode for every DAT type so ASCII line endings are
            // emitted exactly as COMTRADE CRLF on every platform.
            std::ios_base::openmode mode = std::ios::out | std::ios::binary;

            if (detail::isBinaryDataType(cfg_.data_type)) {
                // 预计算数字量需要多少个 16-bit word (向上取整)
                digital_word_count_ = detail::digitalWordCount(cfg_.digital_channels.size());
                row_buffer_.resize(detail::binaryRowSize(
                    cfg_.data_type, cfg_.analog_channels.size(), cfg_.digital_channels.size()));
            } else {
                digital_word_count_ = 0;
                row_buffer_.clear();
            }

            owned_file_.open(dat_filepath, mode);
            output_ = &owned_file_;
            return owned_file_.is_open();
        }

        void open(std::ostream& output) {
            close(); output_ = &output; current_row_index_ = 1;
            digital_word_count_ = detail::digitalWordCount(cfg_.digital_channels.size());
            if (detail::isBinaryDataType(cfg_.data_type)) row_buffer_.resize(detail::binaryRowSize(
                cfg_.data_type, cfg_.analog_channels.size(), cfg_.digital_channels.size()));
        }
        void pushRow(uint64_t timestamp_us,
                     const std::vector<double> &analog_values,
                     const std::vector<bool> &digital_values) {
            if (timestamp_us > static_cast<uint64_t>(std::numeric_limits<int64_t>::max() / 1000))
                throw std::overflow_error("Timestamp exceeds nanosecond range");
            pushRowNanoseconds(static_cast<int64_t>(timestamp_us * 1000), analog_values, digital_values);
        }
        void pushRowNanoseconds(int64_t timestamp_ns,
                     const std::vector<double>& analog_values, const std::vector<bool>& digital_values) {
            // 输入时间是相对开始时刻的微秒偏移，DAT 第二列按 TIMEMULT 反算为原始整数。
            // analog_values/digital_values 的数量必须与 CFG 声明一致。
            if (!output_ || !*output_) throw std::runtime_error("DAT stream not open.");
            if (!std::isfinite(cfg_.time_multiplier) || cfg_.time_multiplier <= 0.0) {
                throw std::invalid_argument("COMTRADE time multiplier must be positive and finite.");
            }
            if (analog_values.size() != cfg_.analog_channels.size() ||
                digital_values.size() != cfg_.digital_channels.size()) {
                throw std::invalid_argument("Sample value counts must match the declared COMTRADE channels.");
            }

            const long double raw = static_cast<long double>(timestamp_ns) / (cfg_.time_multiplier * 1000.0L);
            const auto limit = cfg_.data_type == DataType::ASCII ? static_cast<long double>(std::numeric_limits<int64_t>::max())
                                                                : static_cast<long double>(std::numeric_limits<uint32_t>::max());
            if (!std::isfinite(raw) || raw < 0 || std::round(raw) > limit) {
                throw std::overflow_error("COMTRADE raw timestamp exceeds uint32 range.");
            }

            const auto raw_timestamp = static_cast<uint64_t>(std::round(raw));
            if (cfg_.data_type == DataType::ASCII) {
                writeAsciiRow(raw_timestamp, analog_values, digital_values);
            } else {
                writeBinaryRow(static_cast<uint32_t>(raw_timestamp), analog_values, digital_values);
            }

            if (!stream()) throw std::runtime_error("DAT write failed");
            current_row_index_++;
        }

        void close() {
            if (owned_file_.is_open()) { owned_file_.flush(); owned_file_.close(); }
            output_ = nullptr;
        }

    private:
        void writeAsciiRow(uint64_t ts_us, const std::vector<double> &a_vals, const std::vector<bool> &d_vals) {
            stream() << current_row_index_ << "," << ts_us;
            for (size_t i = 0; i < cfg_.analog_channels.size(); ++i) {
                const auto &ch = cfg_.analog_channels[i];
                // 工程值写回原始整数：raw = round((value - b) / a)。
                stream() << ",";
                if (std::isnan(a_vals[i])) {
                    if (cfg_.version != StandardVersion::V1991) stream() << "99999";
                } else {
                    const auto raw = checkedIntegralAnalog<std::int32_t>(a_vals[i], ch);
                    if (cfg_.version != StandardVersion::V1991 && raw == 99999)
                        throw std::invalid_argument("Analog value collides with missing-data marker");
                    stream() << raw;
                }
            }
            for (const bool val: d_vals) stream() << "," << (val ? 1 : 0);
            stream() << "\r\n";
        }

        // 核心性能点：单行二进制直接打包与落盘
        void writeBinaryRow(uint32_t ts_us, const std::vector<double> &a_vals, const std::vector<bool> &d_vals) {
            char *ptr = row_buffer_.data();

            // 二进制行布局固定为：序号、时间戳、模拟量数组、数字量 word 数组。
            detail::writeUint32LittleEndian(ptr, current_row_index_);
            detail::writeUint32LittleEndian(ptr, ts_us);

            // 2. 写入模拟量
            for (size_t i = 0; i < cfg_.analog_channels.size(); ++i) {
                const auto &ch = cfg_.analog_channels[i];

                if (cfg_.data_type == DataType::BINARY) {
                    detail::writeInt16LittleEndian(ptr, encodedInt16(a_vals[i], ch));
                } else if (cfg_.data_type == DataType::BINARY32) {
                    detail::writeInt32LittleEndian(ptr, encodedInt32(a_vals[i], ch));
                } else {
                    detail::writeFloat32LittleEndian(ptr, encodedFloat(a_vals[i], ch));
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
                detail::writeUint16LittleEndian(ptr, word);
            }

            // 4. 单次 IO 刷入流中
            stream().write(row_buffer_.data(), static_cast<std::streamsize>(row_buffer_.size()));
        }

        int16_t encodedInt16(double value, const AnalogChannel& c) const {
            const int16_t missing = cfg_.version == StandardVersion::V1991 ? -1 : -32768;
            if (std::isnan(value)) return missing;
            const auto raw = checkedIntegralAnalog<int16_t>(value, c);
            if (raw == missing) throw std::invalid_argument("Analog value collides with missing-data marker");
            return raw;
        }
        static int32_t encodedInt32(double value, const AnalogChannel& c) {
            if (std::isnan(value)) return std::numeric_limits<int32_t>::lowest();
            const auto raw = checkedIntegralAnalog<int32_t>(value, c);
            if (raw == std::numeric_limits<int32_t>::lowest()) throw std::invalid_argument("Analog value collides with missing-data marker");
            return raw;
        }
        static float encodedFloat(double value, const AnalogChannel& c) {
            if (std::isnan(value)) return -std::numeric_limits<float>::max();
            const auto raw = checkedFloatAnalog(value, c);
            if (raw == -std::numeric_limits<float>::max()) throw std::invalid_argument("Analog value collides with missing-data marker");
            return raw;
        }
        static double rawAnalogValue(const double engineering_value, const AnalogChannel &channel) {
            if (!std::isfinite(engineering_value) || !std::isfinite(channel.a) || channel.a == 0.0 ||
                !std::isfinite(channel.b)) {
                throw std::invalid_argument("Analog values and coefficients must be finite and scale must be non-zero.");
            }
            const double raw_value = (engineering_value - channel.b) / channel.a;
            if (!std::isfinite(raw_value)) {
                throw std::overflow_error("COMTRADE analog value cannot be represented.");
            }
            return raw_value;
        }

        template<typename Integer>
        static Integer checkedIntegralAnalog(const double engineering_value, const AnalogChannel &channel) {
            const double rounded = std::round(rawAnalogValue(engineering_value, channel));
            if (rounded < static_cast<double>(std::numeric_limits<Integer>::lowest()) ||
                rounded > static_cast<double>(std::numeric_limits<Integer>::max())) {
                throw std::overflow_error("COMTRADE analog value exceeds the selected DAT integer range.");
            }
            return static_cast<Integer>(rounded);
        }

        static float checkedFloatAnalog(const double engineering_value, const AnalogChannel &channel) {
            const double raw_value = rawAnalogValue(engineering_value, channel);
            if (raw_value < -static_cast<double>(std::numeric_limits<float>::max()) ||
                raw_value > static_cast<double>(std::numeric_limits<float>::max())) {
                throw std::overflow_error("COMTRADE analog value exceeds the FLOAT32 range.");
            }
            return static_cast<float>(raw_value);
        }
    };
} // namespace comtrade

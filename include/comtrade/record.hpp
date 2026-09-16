/**
 * @file record.hpp
 * @brief Unified COMTRADE record object for parsing, manipulation, and writing.
 * @author 王广博
 */
#pragma once

#include "types.hpp"
#include "utils.hpp"
#include "cfg_io.hpp"
#include "binary_io.hpp"
#include "stream_writer.hpp"
#include "text_encoding.hpp"
#include <fstream>
#include <cmath>
#include <iomanip>
#include <limits>

namespace comtrade {
    // Record 持有完整 CFG 和全部波形数据，适合编辑、转储和小中型文件的整体处理。
    // 大文件或恒定内存场景应改用 StreamReader/StreamWriter。
    class Record {
    public:
        Record() = default;

        ~Record() = default;

        // ==========================================
        // 1. 数据访问接口
        // ==========================================
        [[nodiscard]] inline const CfgData &getCfg() const { return cfg_; }
        [[nodiscard]] inline const RecordData &getData() const { return data_; }

        // 提供可变引用，允许用户直接在内存中修改配置或数据
        inline CfgData &getMutableCfg() { return cfg_; }
        inline RecordData &getMutableData() { return data_; }

        // ==========================================
        // 2. 解析方法 (Read)
        // ==========================================
        inline bool parseCfg(const std::string &cfg_filepath) {
            CfgData parsed_cfg;
            if (!detail::parseCfgFile(cfg_filepath, parsed_cfg)) return false;
            cfg_ = std::move(parsed_cfg);
            return true;
        }

        inline bool parseDat(const std::string &dat_filepath) {
            // DAT 的列结构取决于已加载的 CFG，因此调用方应先成功执行 parseCfg。
            if (!std::isfinite(cfg_.time_multiplier) || cfg_.time_multiplier <= 0.0 ||
                cfg_.analog_count < 0 || cfg_.digital_count < 0 ||
                cfg_.analog_channels.size() != static_cast<std::size_t>(cfg_.analog_count) ||
                cfg_.digital_channels.size() != static_cast<std::size_t>(cfg_.digital_count)) {
                return false;
            }

            std::ifstream file(dat_filepath, std::ios::binary);
            if (!file.is_open()) return false;

            data_ = RecordData{}; // 清空旧数据
            data_.analog_values.resize(cfg_.analog_count);
            data_.digital_values.resize(cfg_.digital_count);

            if (cfg_.data_type == DataType::ASCII) return parseAsciiDat(file);
            if (detail::isBinaryDataType(cfg_.data_type)) return parseBinaryDat(file);
            return false;
        }

        // ==========================================
        // 3. 落盘方法 (Write/Save)
        // ==========================================
        [[nodiscard]] inline bool saveCfg(const std::string &filepath) const {
            if (!std::isfinite(cfg_.time_multiplier) || cfg_.time_multiplier <= 0.0 ||
                cfg_.timestamp_fractional_digits > 9) {
                return false;
            }
            if (cfg_.version == StandardVersion::V2013 &&
                (cfg_.time_code.empty() || cfg_.local_code.empty() || cfg_.time_quality_code.empty() ||
                 cfg_.leap_second < 0 || cfg_.leap_second > 3)) {
                return false;
            }

            std::ofstream out(filepath, std::ios::binary);
            if (!out.is_open()) return false;
            out << std::setprecision(15);

            out << cfg_.station_name << "," << cfg_.rec_dev_id << "," << static_cast<int>(cfg_.version) << "\r\n";
            out << cfg_.total_channels << "," << cfg_.analog_count << "A," << cfg_.digital_count << "D\r\n";

            for (const auto &ac: cfg_.analog_channels) {
                out << ac.index << "," << ac.id << "," << ac.phase << "," << ac.ccbm << ","
                        << ac.uu << "," << ac.a << "," << ac.b << "," << ac.skew << ","
                        << ac.min << "," << ac.max << "," << ac.primary << "," << ac.secondary << "," << ac.ps << "\r\n";
            }

            for (const auto &dc: cfg_.digital_channels) {
                out << dc.index << "," << dc.id << "," << dc.phase << "," << dc.ccbm << "," << dc.normal_state << "\r\n";
            }

            out << cfg_.line_frequency << "\r\n";
            // 调用方未显式配置采样率时，仅依据前两个时间戳推导一个固定采样率段。
            auto sample_rates = cfg_.sample_rates;
            if (sample_rates.empty()) {
                double sample_rate = 0.0;
                if (data_.timestamp.size() > 1 && data_.timestamp[1] != data_.timestamp[0]) {
                    sample_rate = 1000000.0 / (data_.timestamp[1] - data_.timestamp[0]);
                }
                sample_rates.push_back({sample_rate, static_cast<uint32_t>(data_.timestamp.size())});
            }

            // variable_sample_rate 保留 nrates=0 的语义；采样段本身仍全部写在后续各行。
            out << (cfg_.variable_sample_rate ? 0U : sample_rates.size()) << "\r\n";
            for (const auto& sample_rate : sample_rates) {
                out << sample_rate.samples_per_second << "," << sample_rate.end_sample << "\r\n";
            }
            out << utils::formatTime(cfg_.start_time, cfg_.timestamp_fractional_digits) << "\r\n";
            out << utils::formatTime(cfg_.trigger_time, cfg_.timestamp_fractional_digits) << "\r\n";
            out << DataTypeUtils::ToString(cfg_.data_type) << "\r\n";

            if (cfg_.version != StandardVersion::V1991) {
                out << cfg_.time_multiplier << "\r\n";
            }
            if (cfg_.version == StandardVersion::V2013) {
                out << cfg_.time_code << "," << cfg_.local_code << "\r\n";
                out << cfg_.time_quality_code << "," << cfg_.leap_second << "\r\n";
            }

            return true;
        }

        [[nodiscard]] inline bool saveDat(const std::string &filepath) const {
            if (!std::isfinite(cfg_.time_multiplier) || cfg_.time_multiplier <= 0.0) return false;

            const auto num_samples = data_.timestamp.size();
            if (cfg_.analog_channels.size() != static_cast<std::size_t>(cfg_.analog_count) ||
                cfg_.digital_channels.size() != static_cast<std::size_t>(cfg_.digital_count) ||
                data_.analog_values.size() != cfg_.analog_channels.size() ||
                data_.digital_values.size() != cfg_.digital_channels.size()) {
                return false;
            }
            for (const auto& values : data_.analog_values) {
                if (values.size() != num_samples) return false;
            }
            for (const auto& values : data_.digital_values) {
                if (values.size() != num_samples) return false;
            }

            try {
                StreamWriter writer(cfg_);
                if (!writer.open(filepath)) return false;

                std::vector<double> analog_row(cfg_.analog_channels.size());
                std::vector<bool> digital_row(cfg_.digital_channels.size());
                for (std::size_t sample = 0; sample < num_samples; ++sample) {
                    for (std::size_t channel = 0; channel < analog_row.size(); ++channel) {
                        analog_row[channel] = data_.analog_values[channel][sample];
                    }
                    for (std::size_t channel = 0; channel < digital_row.size(); ++channel) {
                        digital_row[channel] = data_.digital_values[channel][sample];
                    }
                    writer.pushRow(data_.timestamp[sample], analog_row, digital_row);
                }
                writer.close();
            } catch (const std::exception&) {
                return false;
            }
            return true;
        }

        // ==========================================
        // 4. 数据构造与操作方法 (Manipulation)
        // ==========================================
        inline void setStationAndDevice(const std::string &station, const std::string &device,
                                        StandardVersion version = StandardVersion::V1999) {
            cfg_.station_name = station;
            cfg_.rec_dev_id = device;
            cfg_.version = version;
        }

        inline void addAnalogChannel(const AnalogChannel &ch) {
            cfg_.analog_channels.push_back(ch);
            cfg_.analog_count++;
            cfg_.total_channels++;
        }

        inline void addDigitalChannel(const DigitalChannel &ch) {
            cfg_.digital_channels.push_back(ch);
            cfg_.digital_count++;
            cfg_.total_channels++;
        }

        inline void addSample(const uint32_t timestamp_us,
                              const std::vector<double> &analog_reals,
                              const std::vector<bool> &digital_vals) {
            // timestamp_us 是相对 cfg_.start_time 的偏移，不是 Unix epoch 时间。
            // 调用方应为每个已声明通道提供一个值，缺少的列不会被自动补零。
            data_.timestamp.push_back(timestamp_us);

            if (data_.analog_values.empty() && cfg_.analog_count > 0) {
                data_.analog_values.resize(cfg_.analog_count);
            }
            if (data_.digital_values.empty() && cfg_.digital_count > 0) {
                data_.digital_values.resize(cfg_.digital_count);
            }

            for (size_t i = 0; i < analog_reals.size() && i < static_cast<size_t>(cfg_.analog_count); ++i) {
                data_.analog_values[i].push_back(analog_reals[i]);
            }
            for (size_t i = 0; i < digital_vals.size() && i < static_cast<size_t>(cfg_.digital_count); ++i) {
                data_.digital_values[i].push_back(digital_vals[i]);
            }
        }

        /**
         * @brief 设置 COMTRADE 记录的开始时间和触发时间
         * @param start_time   记录开始时间 (纳秒级 TimePoint)
         * @param trigger_time 触发动作时间 (纳秒级 TimePoint)
         */
        inline void setTimestamps(const TimePoint &start_time, const TimePoint &trigger_time) {
            cfg_.start_time = start_time;
            cfg_.trigger_time = trigger_time;
        }

        /**
         * @brief 设置 COMTRADE 记录的开始时间和触发时间 (字符串解析版)
         * @param start_time_str   记录开始时间 (格式: dd/mm/yyyy,hh:mm:ss.ssssss)
         * @param trigger_time_str 触发动作时间 (格式: dd/mm/yyyy,hh:mm:ss.ssssss)
         */
        inline void setTimestamps(const std::string &start_time_str, const std::string &trigger_time_str) {
            cfg_.start_time = utils::parseTime(start_time_str);
            cfg_.trigger_time = utils::parseTime(trigger_time_str);
        }

    private:
        bool appendDecodedSample(const std::uint64_t raw_timestamp,
                                 const std::vector<double>& analog_values,
                                 const std::vector<bool>& digital_values) {
            // RecordData 延续公开 API：保存按 TIMEMULT 换算后的 uint32 微秒偏移。
            const long double timestamp_us = static_cast<long double>(raw_timestamp) *
                                             static_cast<long double>(cfg_.time_multiplier);
            if (!std::isfinite(timestamp_us) || timestamp_us < 0.0L ||
                timestamp_us > static_cast<long double>(std::numeric_limits<std::uint32_t>::max())) {
                return false;
            }

            data_.timestamp.push_back(static_cast<std::uint32_t>(std::llround(timestamp_us)));
            for (std::size_t channel = 0; channel < analog_values.size(); ++channel) {
                data_.analog_values[channel].push_back(analog_values[channel]);
            }
            for (std::size_t channel = 0; channel < digital_values.size(); ++channel) {
                data_.digital_values[channel].push_back(digital_values[channel]);
            }
            return true;
        }

        bool parseAsciiDat(std::istream& file) {
            const auto analog_count = static_cast<std::size_t>(cfg_.analog_count);
            const auto digital_count = static_cast<std::size_t>(cfg_.digital_count);
            std::vector<double> analog_values(analog_count);
            std::vector<bool> digital_values(digital_count);

            std::string line;
            bool first_line = true;
            while (std::getline(file, line)) {
                if (first_line) {
                    detail::stripUtf8Bom(line);
                    first_line = false;
                }
                const auto tokens = utils::split(line);
                if (tokens.size() < 2U + analog_count + digital_count) continue;

                try {
                    const auto raw_timestamp = std::stoull(tokens[1]);
                    for (std::size_t channel = 0; channel < analog_count; ++channel) {
                        const auto raw_value = std::stod(tokens[2U + channel]);
                        const auto& definition = cfg_.analog_channels[channel];
                        analog_values[channel] = raw_value * definition.a + definition.b;
                    }
                    for (std::size_t channel = 0; channel < digital_count; ++channel) {
                        digital_values[channel] =
                            std::stoul(tokens[2U + analog_count + channel]) != 0U;
                    }
                    appendDecodedSample(raw_timestamp, analog_values, digital_values);
                } catch (const std::exception&) {
                    // 与流式读取一致：坏行不影响后续完整采样点。
                    continue;
                }
            }
            return true;
        }

        bool parseBinaryDat(std::istream& file) {
            const auto analog_count = static_cast<std::size_t>(cfg_.analog_count);
            const auto digital_count = static_cast<std::size_t>(cfg_.digital_count);
            const auto row_size = detail::binaryRowSize(cfg_.data_type, analog_count, digital_count);
            std::vector<char> encoded_row(row_size);
            std::vector<double> analog_values(analog_count);
            std::vector<bool> digital_values(digital_count);

            while (file.read(encoded_row.data(), static_cast<std::streamsize>(encoded_row.size()))) {
                const char* cursor = encoded_row.data();
                static_cast<void>(detail::readUint32LittleEndian(cursor)); // sample number
                const auto raw_timestamp = detail::readUint32LittleEndian(cursor);

                for (std::size_t channel = 0; channel < analog_count; ++channel) {
                    double raw_value = 0.0;
                    if (cfg_.data_type == DataType::BINARY) {
                        raw_value = detail::readInt16LittleEndian(cursor);
                    } else if (cfg_.data_type == DataType::BINARY32) {
                        raw_value = detail::readInt32LittleEndian(cursor);
                    } else {
                        raw_value = detail::readFloat32LittleEndian(cursor);
                    }
                    const auto& definition = cfg_.analog_channels[channel];
                    analog_values[channel] = raw_value * definition.a + definition.b;
                }

                for (std::size_t word_index = 0;
                     word_index < detail::digitalWordCount(digital_count);
                     ++word_index) {
                    const auto word = detail::readUint16LittleEndian(cursor);
                    for (std::size_t bit = 0; bit < 16U; ++bit) {
                        const auto channel = word_index * 16U + bit;
                        if (channel >= digital_count) break;
                        digital_values[channel] = (word & (std::uint16_t{1} << bit)) != 0U;
                    }
                }

                appendDecodedSample(raw_timestamp, analog_values, digital_values);
            }
            return true;
        }

        CfgData cfg_;
        RecordData data_;
    };
} // namespace comtrade

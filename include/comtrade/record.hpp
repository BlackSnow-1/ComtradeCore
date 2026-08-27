/**
 * @file record.hpp
 * @brief Unified COMTRADE record object for parsing, manipulation, and writing.
 * @author 王广博
 */
#pragma once

#include "types.hpp"
#include "utils.hpp"
#include "cfg_io.hpp"
#include <fstream>
#include <iostream>
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
            if (cfg_.data_type != DataType::ASCII) {
                std::cerr << "[COMTRADE] Currently only ASCII is supported for parsing.\n";
                return false;
            }

            std::ifstream file(dat_filepath);
            if (!file.is_open()) return false;

            data_ = RecordData{}; // 清空旧数据
            data_.analog_values.resize(cfg_.analog_count);
            data_.digital_values.resize(cfg_.digital_count);

            std::string line;
            while (std::getline(file, line)) {
                auto tokens = utils::split(line);
                if (tokens.size() < static_cast<size_t>(2) + cfg_.analog_count + cfg_.digital_count) continue;

                // TIMEMULT 的单位是“微秒/原始时间单位”。内存中统一保存微秒偏移。
                const auto raw_timestamp = std::stoul(tokens[1]);
                const auto timestamp_us = std::llround(raw_timestamp * cfg_.time_multiplier);
                if (timestamp_us < 0 || timestamp_us > std::numeric_limits<uint32_t>::max()) continue;
                data_.timestamp.push_back(static_cast<uint32_t>(timestamp_us));

                for (int i = 0; i < cfg_.analog_count; ++i) {
                    double raw_val = std::stod(tokens[2 + i]);
                    double real_val = raw_val * cfg_.analog_channels[i].a + cfg_.analog_channels[i].b;
                    data_.analog_values[i].push_back(real_val);
                }

                for (int i = 0; i < cfg_.digital_count; ++i) {
                    const auto token_index = static_cast<std::size_t>(2 + cfg_.analog_count + i);
                    data_.digital_values[i].push_back(std::stoul(tokens[token_index]) != 0);
                }
            }
            return true;
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
            if (cfg_.data_type != DataType::ASCII) return false;
            if (!std::isfinite(cfg_.time_multiplier) || cfg_.time_multiplier <= 0.0) return false;

            std::ofstream out(filepath, std::ios::binary);
            if (!out.is_open()) return false;

            size_t num_samples = data_.timestamp.size();
            for (size_t i = 0; i < num_samples; ++i) {
                // DAT 落盘规则与读取互逆：raw = round(timestamp_us / TIMEMULT)。
                const auto raw_timestamp = std::llround(data_.timestamp[i] / cfg_.time_multiplier);
                if (raw_timestamp < 0 || raw_timestamp > std::numeric_limits<uint32_t>::max()) return false;
                out << (i + 1) << "," << raw_timestamp;

                for (size_t j = 0; j < static_cast<size_t>(cfg_.analog_count); ++j) {
                    double real_val = data_.analog_values[j][i];
                    double a = cfg_.analog_channels[j].a;
                    double b = cfg_.analog_channels[j].b;

                    int raw_val = (a != 0.0) ? static_cast<int>(std::round((real_val - b) / a)) : 0;
                    out << "," << raw_val;
                }

                for (size_t j = 0; j < static_cast<size_t>(cfg_.digital_count); ++j) {
                    out << "," << (data_.digital_values[j][i] ? 1 : 0);
                }
                out << "\r\n";
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
        CfgData cfg_;
        RecordData data_;
    };
} // namespace comtrade

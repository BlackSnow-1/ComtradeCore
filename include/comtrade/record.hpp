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
#include "stream_reader.hpp"
#include <sstream>
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
            warnings_ = cfg_.warnings;
            return true;
        }

        inline bool parseDat(const std::string &dat_filepath) {
            StreamReader reader(cfg_, options_);
            std::ifstream input(dat_filepath, std::ios::binary);
            if (!input) return false;
            return readSamples(reader, [&](const auto& callback) { return reader.processStream(input, callback); });
        }
        const Warnings& getWarnings() const noexcept { return warnings_; }
        void setReadOptions(ReadOptions options) { options_ = options; }
        const std::string& getHeader() const noexcept { return header_; }
        const std::string& getInformation() const noexcept { return information_; }
        void setHeader(std::string text) { header_ = std::move(text); }
        void setInformation(std::string text) { information_ = std::move(text); }
        bool loadCff(const std::string& path) {
            try {
                StreamReader reader(path, options_);
                if (!reader.isCff()) return false;
                Record loaded; loaded.cfg_ = reader.getCfg(); loaded.options_ = options_;
                loaded.header_ = reader.getHeader(); loaded.information_ = reader.getInformation();
                if (!loaded.readSamples(reader, [&](const auto& callback) { return reader.processCffStream(callback); })) return false;
                *this = std::move(loaded); return true;
            } catch (const std::exception&) { return false; }
        }
        bool saveCff(const std::string& path) const {
            if (cfg_.version != StandardVersion::V2013) return false;
            std::ostringstream cfg, dat(std::ios::out | std::ios::binary);
            if (!writeCfg(cfg) || !writeDat(dat)) return false;
            std::ofstream out(path, std::ios::binary);
            if (!out) return false;
            out << "--- file type: CFG ---\r\n" << cfg.str();
            out << "--- file type: INF ---\r\n" << information_;
            if (!information_.empty() && information_.back() != '\n') out << "\r\n";
            out << "--- file type: HDR ---\r\n" << header_;
            if (!header_.empty() && header_.back() != '\n') out << "\r\n";
            out << "--- file type: DAT " << (cfg_.data_type == DataType::ASCII ? "ASCII" : "BINARY");
            if (cfg_.data_type != DataType::ASCII) out << ":" << dat.str().size();
            out << " ---\r\n" << dat.str(); out.flush(); return bool(out);
        }

        // ==========================================
        // 3. 落盘方法 (Write/Save)
        // ==========================================
        [[nodiscard]] inline bool saveCfg(const std::string &filepath) const {
            std::ofstream out(filepath, std::ios::binary);
            if (!out) return false;
            const bool ok = writeCfg(out); out.flush(); return ok && bool(out);
        }
        bool writeCfg(std::ostream& out) const {
            if (!std::isfinite(cfg_.time_multiplier) || cfg_.time_multiplier <= 0.0 ||
                cfg_.timestamp_fractional_digits > 9) {
                return false;
            }
            if (cfg_.version == StandardVersion::V2013 &&
                (cfg_.time_code.empty() || cfg_.local_code.empty() || cfg_.time_quality_code.empty() ||
                 cfg_.leap_second < 0 || cfg_.leap_second > 3)) {
                return false;
            }

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
            out << utils::formatTime(cfg_.start_time, cfg_.timestamp_fractional_digits, cfg_.utc_offset_known, cfg_.utc_offset_minutes) << "\r\n";
            out << utils::formatTime(cfg_.trigger_time, cfg_.timestamp_fractional_digits, cfg_.utc_offset_known, cfg_.utc_offset_minutes) << "\r\n";
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
            std::ofstream out(filepath, std::ios::binary);
            if (!out) return false;
            const bool ok = writeDat(out); out.flush(); return ok && bool(out);
        }
        bool writeDat(std::ostream& output) const {
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
                writer.open(output);

                std::vector<double> analog_row(cfg_.analog_channels.size());
                std::vector<bool> digital_row(cfg_.digital_channels.size());
                for (std::size_t sample = 0; sample < num_samples; ++sample) {
                    for (std::size_t channel = 0; channel < analog_row.size(); ++channel) {
                        analog_row[channel] = data_.analog_values[channel][sample];
                    }
                    for (std::size_t channel = 0; channel < digital_row.size(); ++channel) {
                        digital_row[channel] = data_.digital_values[channel][sample];
                    }
                    if (data_.time_offset_ns.size() == num_samples &&
                        static_cast<uint64_t>(data_.time_offset_ns[sample] / 1000) == data_.timestamp[sample])
                        writer.pushRowNanoseconds(data_.time_offset_ns[sample], analog_row, digital_row);
                    else writer.pushRow(data_.timestamp[sample], analog_row, digital_row);
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

        inline void addSample(const uint64_t timestamp_us,
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
            cfg_.utc_offset_known = cfg_.version == StandardVersion::V2013 && utils::parseUtcOffset(cfg_.time_code, cfg_.utc_offset_minutes);
            cfg_.start_time = utils::parseTime(start_time_str, cfg_.utc_offset_known, cfg_.utc_offset_minutes);
            cfg_.trigger_time = utils::parseTime(trigger_time_str, cfg_.utc_offset_known, cfg_.utc_offset_minutes);
        }

    private:
        template<class Processor>
        bool readSamples(const StreamReader& reader, Processor process) {
            RecordData decoded;
            decoded.analog_values.resize(cfg_.analog_count); decoded.digital_values.resize(cfg_.digital_count);
            try {
                process([&](const SampleRow& row) {
                    decoded.timestamp.push_back(row.timestamp_us);
                    decoded.time_offset_ns.push_back(row.time_offset.count());
                    for (size_t i=0;i<row.analog_values.size();++i) decoded.analog_values[i].push_back(row.analog_values[i]);
                    for (size_t i=0;i<row.digital_values.size();++i) decoded.digital_values[i].push_back(row.digital_values[i]);
                });
            } catch (const std::exception&) { return false; }
            data_ = std::move(decoded); warnings_ = reader.getWarnings(); return true;
        }
        ReadOptions options_;
        Warnings warnings_;
        std::string header_, information_;
        CfgData cfg_;
        RecordData data_;
    };
} // namespace comtrade

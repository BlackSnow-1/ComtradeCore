/**
 * @file java_api.hpp
 * @brief Java-friendly facade for the SWIG/JNI binding.
 */
#pragma once

#include "comtrade/record.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/**
 * SWIG only exposes this facade to Java. Keeping the facade separate prevents
 * implementation details such as std::chrono and std::vector<bool> from
 * leaking into the generated Java API.
 */
class ComtradeNativeRecord {
public:
    ComtradeNativeRecord() = default;

    void clear() {
        record_ = comtrade::Record{};
    }

    void setStationAndDevice(const std::string& station,
                             const std::string& device,
                             const int version_year) {
        if (station.empty() || device.empty()) {
            throw std::invalid_argument("station and device must not be empty");
        }
        record_.setStationAndDevice(station, device, toStandardVersion(version_year));
    }

    void setTimestamps(const std::string& start_time, const std::string& trigger_time) {
        if (start_time.empty() || trigger_time.empty()) {
            throw std::invalid_argument("start time and trigger time must not be empty");
        }
        record_.setTimestamps(start_time, trigger_time);
    }

    void setTimeMultiplier(const double multiplier) {
        if (!std::isfinite(multiplier) || multiplier <= 0.0) {
            throw std::invalid_argument("time multiplier must be finite and greater than zero");
        }
        record_.getMutableCfg().time_multiplier = multiplier;
    }

    void setTimestampFractionalDigits(const int digits) {
        if (digits < 0 || digits > 9) {
            throw std::invalid_argument("timestamp fractional digits must be in [0, 9]");
        }
        record_.getMutableCfg().timestamp_fractional_digits = static_cast<std::uint8_t>(digits);
    }

    void set2013TimeMetadata(const std::string& time_code,
                             const std::string& local_code,
                             const std::string& time_quality_code,
                             const int leap_second) {
        if (time_code.empty() || local_code.empty() || time_quality_code.empty()) {
            throw std::invalid_argument("2013 time metadata fields must not be empty");
        }
        if (leap_second < 0 || leap_second > 3) {
            throw std::invalid_argument("leap second indicator must be in [0, 3]");
        }

        auto& cfg = record_.getMutableCfg();
        cfg.time_code = time_code;
        cfg.local_code = local_code;
        cfg.time_quality_code = time_quality_code;
        cfg.leap_second = leap_second;
    }

    void addAnalogChannel(const int index,
                          const std::string& id,
                          const std::string& phase,
                          const std::string& unit,
                          const double scale,
                          const double offset) {
        if (index <= 0 || id.empty()) {
            throw std::invalid_argument("analog channel index must be positive and id must not be empty");
        }
        if (!std::isfinite(scale) || scale == 0.0 || !std::isfinite(offset)) {
            throw std::invalid_argument("analog channel scale must be non-zero and coefficients must be finite");
        }

        comtrade::AnalogChannel channel{};
        channel.index = index;
        channel.id = id;
        channel.phase = phase;
        channel.uu = unit;
        channel.a = scale;
        channel.b = offset;
        record_.addAnalogChannel(channel);
    }

    void addDigitalChannel(const int index,
                           const std::string& id,
                           const std::string& phase,
                           const int normal_state) {
        if (index <= 0 || id.empty()) {
            throw std::invalid_argument("digital channel index must be positive and id must not be empty");
        }
        if (normal_state != 0 && normal_state != 1) {
            throw std::invalid_argument("digital channel normal state must be 0 or 1");
        }

        comtrade::DigitalChannel channel{};
        channel.index = index;
        channel.id = id;
        channel.phase = phase;
        channel.normal_state = normal_state;
        record_.addDigitalChannel(channel);
    }

    void addSample(const long long timestamp_us,
                   const std::vector<double>& analog_values,
                   const std::vector<int>& digital_values) {
        if (timestamp_us < 0 ||
            static_cast<unsigned long long>(timestamp_us) > std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("timestamp must fit in an unsigned 32-bit microsecond offset");
        }

        const auto& cfg = record_.getCfg();
        if (analog_values.size() != static_cast<std::size_t>(cfg.analog_count) ||
            digital_values.size() != static_cast<std::size_t>(cfg.digital_count)) {
            throw std::invalid_argument("sample value counts must match the declared channel counts");
        }

        for (const double value : analog_values) {
            if (!std::isfinite(value)) {
                throw std::invalid_argument("analog sample values must be finite");
            }
        }

        std::vector<bool> converted_digital_values;
        converted_digital_values.reserve(digital_values.size());
        for (const int value : digital_values) {
            if (value != 0 && value != 1) {
                throw std::invalid_argument("digital sample values must be 0 or 1");
            }
            converted_digital_values.push_back(value != 0);
        }

        record_.addSample(static_cast<std::uint32_t>(timestamp_us), analog_values, converted_digital_values);
    }

    bool load(const std::string& cfg_path, const std::string& dat_path) {
        // Parse into a temporary object so a failed load does not destroy the
        // record that the Java caller was already using.
        comtrade::Record loaded;
        if (!loaded.parseCfg(cfg_path) || !loaded.parseDat(dat_path)) {
            return false;
        }
        record_ = std::move(loaded);
        return true;
    }

    bool save(const std::string& cfg_path, const std::string& dat_path) const {
        return record_.saveCfg(cfg_path) && record_.saveDat(dat_path);
    }

    std::string stationName() const {
        return record_.getCfg().station_name;
    }

    std::string deviceId() const {
        return record_.getCfg().rec_dev_id;
    }

    int versionYear() const {
        return static_cast<int>(record_.getCfg().version);
    }

    int analogChannelCount() const {
        return record_.getCfg().analog_count;
    }

    int digitalChannelCount() const {
        return record_.getCfg().digital_count;
    }

    long long sampleCount() const {
        return static_cast<long long>(record_.getData().timestamp.size());
    }

    long long timestampAt(const long long sample_index) const {
        const auto index = checkedIndex(sample_index, record_.getData().timestamp.size(), "sample index");
        return static_cast<long long>(record_.getData().timestamp.at(index));
    }

    double analogValueAt(const int channel_index, const long long sample_index) const {
        const auto channel = checkedIndex(channel_index,
                                          record_.getData().analog_values.size(),
                                          "analog channel index");
        const auto& values = record_.getData().analog_values.at(channel);
        const auto sample = checkedIndex(sample_index, values.size(), "sample index");
        return values.at(sample);
    }

    bool digitalValueAt(const int channel_index, const long long sample_index) const {
        const auto channel = checkedIndex(channel_index,
                                          record_.getData().digital_values.size(),
                                          "digital channel index");
        const auto& values = record_.getData().digital_values.at(channel);
        const auto sample = checkedIndex(sample_index, values.size(), "sample index");
        return values.at(sample);
    }

private:
    static comtrade::StandardVersion toStandardVersion(const int version_year) {
        switch (version_year) {
            case 1991:
                return comtrade::StandardVersion::V1991;
            case 1999:
                return comtrade::StandardVersion::V1999;
            case 2013:
                return comtrade::StandardVersion::V2013;
            default:
                throw std::invalid_argument("COMTRADE version must be 1991, 1999, or 2013");
        }
    }

    template<typename Index>
    static std::size_t checkedIndex(const Index index,
                                    const std::size_t size,
                                    const char* description) {
        if (index < 0 || static_cast<unsigned long long>(index) >= size) {
            throw std::out_of_range(std::string(description) + " is out of range");
        }
        return static_cast<std::size_t>(index);
    }

    comtrade::Record record_;
};

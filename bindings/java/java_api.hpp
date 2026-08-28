/**
 * @file java_api.hpp
 * @brief Java-friendly facades for every public ComtradeCore record and stream API.
 */
#pragma once

#include "comtrade/record.hpp"
#include "comtrade/stream_reader.hpp"
#include "comtrade/stream_writer.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace comtrade_java_detail {

template<typename Index>
inline std::size_t checkedIndex(const Index index, const std::size_t size, const char* description) {
    if (index < 0 || static_cast<unsigned long long>(index) >= size) {
        throw std::out_of_range(std::string(description) + " is out of range");
    }
    return static_cast<std::size_t>(index);
}

inline comtrade::StandardVersion toStandardVersion(const int version_year) {
    switch (version_year) {
        case 1991: return comtrade::StandardVersion::V1991;
        case 1999: return comtrade::StandardVersion::V1999;
        case 2013: return comtrade::StandardVersion::V2013;
        default: throw std::invalid_argument("COMTRADE version must be 1991, 1999, or 2013");
    }
}

inline void validateFinite(const double value, const char* description) {
    if (!std::isfinite(value)) throw std::invalid_argument(std::string(description) + " must be finite");
}

inline std::vector<bool> toDigitalValues(const std::vector<int>& values) {
    std::vector<bool> converted;
    converted.reserve(values.size());
    for (const int value : values) {
        if (value != 0 && value != 1) {
            throw std::invalid_argument("digital sample values must be 0 or 1");
        }
        converted.push_back(value != 0);
    }
    return converted;
}

inline void validateSampleValues(const comtrade::CfgData& cfg,
                                 const std::vector<double>& analog_values,
                                 const std::vector<int>& digital_values) {
    if (analog_values.size() != cfg.analog_channels.size() ||
        digital_values.size() != cfg.digital_channels.size()) {
        throw std::invalid_argument("sample value counts must match the declared channel counts");
    }
    for (const double value : analog_values) validateFinite(value, "analog sample value");
}

inline std::string formatCfgTime(const comtrade::CfgData& cfg, const comtrade::TimePoint& value) {
    return comtrade::utils::formatTime(value, cfg.timestamp_fractional_digits);
}

}  // namespace comtrade_java_detail

/**
 * SWIG exposes this facade instead of chrono and vector<bool> based C++ types.
 * Index arguments selecting a vector element are zero-based; channel.index is
 * the one-based identifier serialized into the COMTRADE CFG file.
 */
class ComtradeNativeRecord {
public:
    ComtradeNativeRecord() = default;

    void clear() { record_ = comtrade::Record{}; }

    bool parseCfg(const std::string& cfg_path) { return record_.parseCfg(cfg_path); }
    bool parseDat(const std::string& dat_path) { return record_.parseDat(dat_path); }
    bool saveCfg(const std::string& cfg_path) const { return record_.saveCfg(cfg_path); }
    bool saveDat(const std::string& dat_path) const { return record_.saveDat(dat_path); }

    bool load(const std::string& cfg_path, const std::string& dat_path) {
        comtrade::Record loaded;
        if (!loaded.parseCfg(cfg_path) || !loaded.parseDat(dat_path)) return false;
        record_ = std::move(loaded);
        return true;
    }

    bool save(const std::string& cfg_path, const std::string& dat_path) const {
        return record_.saveCfg(cfg_path) && record_.saveDat(dat_path);
    }

    void setStationAndDevice(const std::string& station, const std::string& device, const int version_year) {
        if (station.empty() || device.empty()) {
            throw std::invalid_argument("station and device must not be empty");
        }
        record_.setStationAndDevice(station, device,
                                    comtrade_java_detail::toStandardVersion(version_year));
    }

    void setStationName(const std::string& value) { record_.getMutableCfg().station_name = value; }
    void setDeviceId(const std::string& value) { record_.getMutableCfg().rec_dev_id = value; }
    void setVersionYear(const int value) {
        record_.getMutableCfg().version = comtrade_java_detail::toStandardVersion(value);
    }

    std::string stationName() const { return cfg().station_name; }
    std::string deviceId() const { return cfg().rec_dev_id; }
    int versionYear() const { return static_cast<int>(cfg().version); }

    void setTimestamps(const std::string& start_time, const std::string& trigger_time) {
        if (start_time.empty() || trigger_time.empty()) {
            throw std::invalid_argument("start time and trigger time must not be empty");
        }
        record_.setTimestamps(start_time, trigger_time);
    }

    std::string startTime() const { return comtrade_java_detail::formatCfgTime(cfg(), cfg().start_time); }
    std::string triggerTime() const { return comtrade_java_detail::formatCfgTime(cfg(), cfg().trigger_time); }

    void setLineFrequency(const double value) {
        comtrade_java_detail::validateFinite(value, "line frequency");
        if (value < 0.0) throw std::invalid_argument("line frequency must not be negative");
        record_.getMutableCfg().line_frequency = value;
    }
    double lineFrequency() const { return cfg().line_frequency; }

    void setDataType(const std::string& value) {
        record_.getMutableCfg().data_type = comtrade::DataTypeUtils::FromString(value);
    }
    std::string dataType() const { return comtrade::DataTypeUtils::ToString(cfg().data_type); }

    void setVariableSampleRate(const bool value) { record_.getMutableCfg().variable_sample_rate = value; }
    bool variableSampleRate() const { return cfg().variable_sample_rate; }

    void setTimeMultiplier(const double multiplier) {
        if (!std::isfinite(multiplier) || multiplier <= 0.0) {
            throw std::invalid_argument("time multiplier must be finite and greater than zero");
        }
        record_.getMutableCfg().time_multiplier = multiplier;
    }
    double timeMultiplier() const { return cfg().time_multiplier; }

    void setTimestampFractionalDigits(const int digits) {
        if (digits < 0 || digits > 9) {
            throw std::invalid_argument("timestamp fractional digits must be in [0, 9]");
        }
        record_.getMutableCfg().timestamp_fractional_digits = static_cast<std::uint8_t>(digits);
    }
    int timestampFractionalDigits() const { return static_cast<int>(cfg().timestamp_fractional_digits); }

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
        auto& value = record_.getMutableCfg();
        value.time_code = time_code;
        value.local_code = local_code;
        value.time_quality_code = time_quality_code;
        value.leap_second = leap_second;
    }
    std::string timeCode() const { return cfg().time_code; }
    std::string localCode() const { return cfg().local_code; }
    std::string timeQualityCode() const { return cfg().time_quality_code; }
    int leapSecond() const { return cfg().leap_second; }

    void addAnalogChannel(const int index,
                          const std::string& id,
                          const std::string& phase,
                          const std::string& ccbm,
                          const std::string& unit,
                          const double scale,
                          const double offset,
                          const double skew,
                          const int minimum,
                          const int maximum,
                          const double primary,
                          const double secondary,
                          const std::string& primary_secondary) {
        record_.addAnalogChannel(makeAnalogChannel(index, id, phase, ccbm, unit, scale, offset, skew,
                                                   minimum, maximum, primary, secondary, primary_secondary));
        if (!record_.getData().timestamp.empty()) {
            record_.getMutableData().analog_values.emplace_back(record_.getData().timestamp.size(), 0.0);
        }
    }

    void addAnalogChannel(const int index,
                          const std::string& id,
                          const std::string& phase,
                          const std::string& unit,
                          const double scale,
                          const double offset) {
        addAnalogChannel(index, id, phase, "", unit, scale, offset, 0.0, 0, 0, 1.0, 1.0, "P");
    }

    void setAnalogChannel(const int position,
                          const int index,
                          const std::string& id,
                          const std::string& phase,
                          const std::string& ccbm,
                          const std::string& unit,
                          const double scale,
                          const double offset,
                          const double skew,
                          const int minimum,
                          const int maximum,
                          const double primary,
                          const double secondary,
                          const std::string& primary_secondary) {
        const auto selected = comtrade_java_detail::checkedIndex(
            position, cfg().analog_channels.size(), "analog channel position");
        record_.getMutableCfg().analog_channels.at(selected) =
            makeAnalogChannel(index, id, phase, ccbm, unit, scale, offset, skew,
                              minimum, maximum, primary, secondary, primary_secondary);
    }

    void removeAnalogChannel(const int position) {
        const auto selected = comtrade_java_detail::checkedIndex(
            position, cfg().analog_channels.size(), "analog channel position");
        auto& value = record_.getMutableCfg();
        value.analog_channels.erase(value.analog_channels.begin() + static_cast<std::ptrdiff_t>(selected));
        --value.analog_count;
        --value.total_channels;
        auto& samples = record_.getMutableData().analog_values;
        if (selected < samples.size()) samples.erase(samples.begin() + static_cast<std::ptrdiff_t>(selected));
    }

    void clearAnalogChannels() {
        auto& value = record_.getMutableCfg();
        value.total_channels -= value.analog_count;
        value.analog_count = 0;
        value.analog_channels.clear();
        record_.getMutableData().analog_values.clear();
    }

    int analogChannelCount() const { return cfg().analog_count; }
    int analogChannelIndex(const int position) const { return analogChannel(position).index; }
    std::string analogChannelId(const int position) const { return analogChannel(position).id; }
    std::string analogChannelPhase(const int position) const { return analogChannel(position).phase; }
    std::string analogChannelCcbm(const int position) const { return analogChannel(position).ccbm; }
    std::string analogChannelUnit(const int position) const { return analogChannel(position).uu; }
    double analogChannelScale(const int position) const { return analogChannel(position).a; }
    double analogChannelOffset(const int position) const { return analogChannel(position).b; }
    double analogChannelSkew(const int position) const { return analogChannel(position).skew; }
    int analogChannelMinimum(const int position) const { return analogChannel(position).min; }
    int analogChannelMaximum(const int position) const { return analogChannel(position).max; }
    double analogChannelPrimary(const int position) const { return analogChannel(position).primary; }
    double analogChannelSecondary(const int position) const { return analogChannel(position).secondary; }
    std::string analogChannelPrimarySecondary(const int position) const { return analogChannel(position).ps; }

    void addDigitalChannel(const int index,
                           const std::string& id,
                           const std::string& phase,
                           const std::string& ccbm,
                           const int normal_state) {
        record_.addDigitalChannel(makeDigitalChannel(index, id, phase, ccbm, normal_state));
        if (!record_.getData().timestamp.empty()) {
            record_.getMutableData().digital_values.emplace_back(
                record_.getData().timestamp.size(), normal_state != 0);
        }
    }

    void addDigitalChannel(const int index,
                           const std::string& id,
                           const std::string& phase,
                           const int normal_state) {
        addDigitalChannel(index, id, phase, "", normal_state);
    }

    void setDigitalChannel(const int position,
                           const int index,
                           const std::string& id,
                           const std::string& phase,
                           const std::string& ccbm,
                           const int normal_state) {
        const auto selected = comtrade_java_detail::checkedIndex(
            position, cfg().digital_channels.size(), "digital channel position");
        record_.getMutableCfg().digital_channels.at(selected) =
            makeDigitalChannel(index, id, phase, ccbm, normal_state);
    }

    void removeDigitalChannel(const int position) {
        const auto selected = comtrade_java_detail::checkedIndex(
            position, cfg().digital_channels.size(), "digital channel position");
        auto& value = record_.getMutableCfg();
        value.digital_channels.erase(value.digital_channels.begin() + static_cast<std::ptrdiff_t>(selected));
        --value.digital_count;
        --value.total_channels;
        auto& samples = record_.getMutableData().digital_values;
        if (selected < samples.size()) samples.erase(samples.begin() + static_cast<std::ptrdiff_t>(selected));
    }

    void clearDigitalChannels() {
        auto& value = record_.getMutableCfg();
        value.total_channels -= value.digital_count;
        value.digital_count = 0;
        value.digital_channels.clear();
        record_.getMutableData().digital_values.clear();
    }

    int digitalChannelCount() const { return cfg().digital_count; }
    int totalChannelCount() const { return cfg().total_channels; }
    int digitalChannelIndex(const int position) const { return digitalChannel(position).index; }
    std::string digitalChannelId(const int position) const { return digitalChannel(position).id; }
    std::string digitalChannelPhase(const int position) const { return digitalChannel(position).phase; }
    std::string digitalChannelCcbm(const int position) const { return digitalChannel(position).ccbm; }
    int digitalChannelNormalState(const int position) const { return digitalChannel(position).normal_state; }

    void addSampleRate(const double samples_per_second, const long long end_sample) {
        record_.getMutableCfg().sample_rates.push_back(makeSampleRate(samples_per_second, end_sample));
    }
    void setSampleRate(const int position, const double samples_per_second, const long long end_sample) {
        const auto selected = comtrade_java_detail::checkedIndex(
            position, cfg().sample_rates.size(), "sample rate position");
        record_.getMutableCfg().sample_rates.at(selected) = makeSampleRate(samples_per_second, end_sample);
    }
    void removeSampleRate(const int position) {
        const auto selected = comtrade_java_detail::checkedIndex(
            position, cfg().sample_rates.size(), "sample rate position");
        auto& rates = record_.getMutableCfg().sample_rates;
        rates.erase(rates.begin() + static_cast<std::ptrdiff_t>(selected));
    }
    void clearSampleRates() { record_.getMutableCfg().sample_rates.clear(); }
    int sampleRateCount() const { return static_cast<int>(cfg().sample_rates.size()); }
    double sampleRateFrequency(const int position) const { return sampleRate(position).samples_per_second; }
    long long sampleRateEndSample(const int position) const {
        return static_cast<long long>(sampleRate(position).end_sample);
    }

    void addSample(const long long timestamp_us,
                   const std::vector<double>& analog_values,
                   const std::vector<int>& digital_values) {
        const auto timestamp = checkedTimestamp(timestamp_us);
        comtrade_java_detail::validateSampleValues(cfg(), analog_values, digital_values);
        record_.addSample(timestamp, analog_values, comtrade_java_detail::toDigitalValues(digital_values));
    }

    void setTimestampAt(const long long sample_index, const long long timestamp_us) {
        const auto selected = comtrade_java_detail::checkedIndex(
            sample_index, record_.getData().timestamp.size(), "sample index");
        record_.getMutableData().timestamp.at(selected) = checkedTimestamp(timestamp_us);
    }

    void setAnalogValueAt(const int channel_index, const long long sample_index, const double value) {
        comtrade_java_detail::validateFinite(value, "analog sample value");
        const auto channel = comtrade_java_detail::checkedIndex(
            channel_index, record_.getData().analog_values.size(), "analog channel index");
        auto& values = record_.getMutableData().analog_values.at(channel);
        const auto sample = comtrade_java_detail::checkedIndex(sample_index, values.size(), "sample index");
        values.at(sample) = value;
    }

    void setDigitalValueAt(const int channel_index, const long long sample_index, const bool value) {
        const auto channel = comtrade_java_detail::checkedIndex(
            channel_index, record_.getData().digital_values.size(), "digital channel index");
        auto& values = record_.getMutableData().digital_values.at(channel);
        const auto sample = comtrade_java_detail::checkedIndex(sample_index, values.size(), "sample index");
        values.at(sample) = value;
    }

    void removeSample(const long long sample_index) {
        const auto selected = comtrade_java_detail::checkedIndex(
            sample_index, record_.getData().timestamp.size(), "sample index");
        auto& data = record_.getMutableData();
        data.timestamp.erase(data.timestamp.begin() + static_cast<std::ptrdiff_t>(selected));
        for (auto& values : data.analog_values) {
            if (selected < values.size()) values.erase(values.begin() + static_cast<std::ptrdiff_t>(selected));
        }
        for (auto& values : data.digital_values) {
            if (selected < values.size()) values.erase(values.begin() + static_cast<std::ptrdiff_t>(selected));
        }
    }

    void clearSamples() { record_.getMutableData() = comtrade::RecordData{}; }
    long long sampleCount() const { return static_cast<long long>(record_.getData().timestamp.size()); }
    long long timestampAt(const long long sample_index) const {
        const auto index = comtrade_java_detail::checkedIndex(
            sample_index, record_.getData().timestamp.size(), "sample index");
        return static_cast<long long>(record_.getData().timestamp.at(index));
    }
    double analogValueAt(const int channel_index, const long long sample_index) const {
        const auto channel = comtrade_java_detail::checkedIndex(
            channel_index, record_.getData().analog_values.size(), "analog channel index");
        const auto& values = record_.getData().analog_values.at(channel);
        const auto sample = comtrade_java_detail::checkedIndex(sample_index, values.size(), "sample index");
        return values.at(sample);
    }
    bool digitalValueAt(const int channel_index, const long long sample_index) const {
        const auto channel = comtrade_java_detail::checkedIndex(
            channel_index, record_.getData().digital_values.size(), "digital channel index");
        const auto& values = record_.getData().digital_values.at(channel);
        const auto sample = comtrade_java_detail::checkedIndex(sample_index, values.size(), "sample index");
        return values.at(sample);
    }

    const comtrade::CfgData& nativeCfg() const { return cfg(); }

private:
    static comtrade::AnalogChannel makeAnalogChannel(const int index,
                                                      const std::string& id,
                                                      const std::string& phase,
                                                      const std::string& ccbm,
                                                      const std::string& unit,
                                                      const double scale,
                                                      const double offset,
                                                      const double skew,
                                                      const int minimum,
                                                      const int maximum,
                                                      const double primary,
                                                      const double secondary,
                                                      const std::string& primary_secondary) {
        if (index <= 0 || id.empty()) {
            throw std::invalid_argument("analog channel index must be positive and id must not be empty");
        }
        if (!std::isfinite(scale) || scale == 0.0 || !std::isfinite(offset) || !std::isfinite(skew) ||
            !std::isfinite(primary) || !std::isfinite(secondary)) {
            throw std::invalid_argument("analog channel coefficients must be finite and scale must be non-zero");
        }
        comtrade::AnalogChannel channel{};
        channel.index = index;
        channel.id = id;
        channel.phase = phase;
        channel.ccbm = ccbm;
        channel.uu = unit;
        channel.a = scale;
        channel.b = offset;
        channel.skew = skew;
        channel.min = minimum;
        channel.max = maximum;
        channel.primary = primary;
        channel.secondary = secondary;
        channel.ps = primary_secondary;
        return channel;
    }

    static comtrade::DigitalChannel makeDigitalChannel(const int index,
                                                        const std::string& id,
                                                        const std::string& phase,
                                                        const std::string& ccbm,
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
        channel.ccbm = ccbm;
        channel.normal_state = normal_state;
        return channel;
    }

    static comtrade::SampleRate makeSampleRate(const double samples_per_second, const long long end_sample) {
        if (!std::isfinite(samples_per_second) || samples_per_second < 0.0) {
            throw std::invalid_argument("sample rate must be finite and not negative");
        }
        if (end_sample < 0 || static_cast<unsigned long long>(end_sample) >
                                  std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("sample rate end sample must fit in an unsigned 32-bit integer");
        }
        return {samples_per_second, static_cast<std::uint32_t>(end_sample)};
    }

    static std::uint32_t checkedTimestamp(const long long timestamp_us) {
        if (timestamp_us < 0 || static_cast<unsigned long long>(timestamp_us) >
                                    std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("timestamp must fit in an unsigned 32-bit microsecond offset");
        }
        return static_cast<std::uint32_t>(timestamp_us);
    }

    const comtrade::CfgData& cfg() const { return record_.getCfg(); }
    const comtrade::AnalogChannel& analogChannel(const int position) const {
        return cfg().analog_channels.at(comtrade_java_detail::checkedIndex(
            position, cfg().analog_channels.size(), "analog channel position"));
    }
    const comtrade::DigitalChannel& digitalChannel(const int position) const {
        return cfg().digital_channels.at(comtrade_java_detail::checkedIndex(
            position, cfg().digital_channels.size(), "digital channel position"));
    }
    const comtrade::SampleRate& sampleRate(const int position) const {
        return cfg().sample_rates.at(comtrade_java_detail::checkedIndex(
            position, cfg().sample_rates.size(), "sample rate position"));
    }

    comtrade::Record record_;
};

class ComtradeNativeRowCallback {
public:
    virtual ~ComtradeNativeRowCallback() = default;
    virtual void onRow(long long index,
                       long long raw_timestamp,
                       long long timestamp_us,
                       long long time_offset_nanoseconds,
                       const std::string& absolute_time,
                       const std::vector<double>& analog_values,
                       const std::vector<int>& digital_values) = 0;
};

class ComtradeNativeStreamReader {
public:
    explicit ComtradeNativeStreamReader(const std::string& cfg_path) : reader_(cfg_path) {}

    long long processDatStream(const std::string& dat_path, ComtradeNativeRowCallback& callback) const {
        const auto count = reader_.processDatStream(dat_path, [&](const comtrade::SampleRow& row) {
            std::vector<int> digital_values;
            digital_values.reserve(row.digital_values.size());
            for (const bool value : row.digital_values) digital_values.push_back(value ? 1 : 0);
            callback.onRow(static_cast<long long>(row.index),
                           checkedUnsignedLongLong(row.raw_timestamp, "raw timestamp"),
                           checkedUnsignedLongLong(row.timestamp_us, "timestamp"),
                           static_cast<long long>(row.time_offset.count()),
                           comtrade::utils::formatTime(
                               row.absolute_time, reader_.getCfg().timestamp_fractional_digits),
                           row.analog_values,
                           digital_values);
        });
        if (count > static_cast<std::size_t>(std::numeric_limits<long long>::max())) {
            throw std::overflow_error("parsed row count exceeds Java long range");
        }
        return static_cast<long long>(count);
    }

    std::string stationName() const { return reader_.getCfg().station_name; }
    std::string deviceId() const { return reader_.getCfg().rec_dev_id; }
    int versionYear() const { return static_cast<int>(reader_.getCfg().version); }
    int analogChannelCount() const { return reader_.getCfg().analog_count; }
    int digitalChannelCount() const { return reader_.getCfg().digital_count; }
    int totalChannelCount() const { return reader_.getCfg().total_channels; }
    std::string dataType() const { return comtrade::DataTypeUtils::ToString(reader_.getCfg().data_type); }
    double lineFrequency() const { return reader_.getCfg().line_frequency; }
    bool variableSampleRate() const { return reader_.getCfg().variable_sample_rate; }
    double timeMultiplier() const { return reader_.getCfg().time_multiplier; }
    int timestampFractionalDigits() const {
        return static_cast<int>(reader_.getCfg().timestamp_fractional_digits);
    }
    std::string timeCode() const { return reader_.getCfg().time_code; }
    std::string localCode() const { return reader_.getCfg().local_code; }
    std::string timeQualityCode() const { return reader_.getCfg().time_quality_code; }
    int leapSecond() const { return reader_.getCfg().leap_second; }
    std::string startTime() const {
        return comtrade_java_detail::formatCfgTime(reader_.getCfg(), reader_.getCfg().start_time);
    }
    std::string triggerTime() const {
        return comtrade_java_detail::formatCfgTime(reader_.getCfg(), reader_.getCfg().trigger_time);
    }

    int analogChannelIndex(const int position) const { return analogChannel(position).index; }
    std::string analogChannelId(const int position) const { return analogChannel(position).id; }
    std::string analogChannelPhase(const int position) const { return analogChannel(position).phase; }
    std::string analogChannelCcbm(const int position) const { return analogChannel(position).ccbm; }
    std::string analogChannelUnit(const int position) const { return analogChannel(position).uu; }
    double analogChannelScale(const int position) const { return analogChannel(position).a; }
    double analogChannelOffset(const int position) const { return analogChannel(position).b; }
    double analogChannelSkew(const int position) const { return analogChannel(position).skew; }
    int analogChannelMinimum(const int position) const { return analogChannel(position).min; }
    int analogChannelMaximum(const int position) const { return analogChannel(position).max; }
    double analogChannelPrimary(const int position) const { return analogChannel(position).primary; }
    double analogChannelSecondary(const int position) const { return analogChannel(position).secondary; }
    std::string analogChannelPrimarySecondary(const int position) const { return analogChannel(position).ps; }

    int digitalChannelIndex(const int position) const { return digitalChannel(position).index; }
    std::string digitalChannelId(const int position) const { return digitalChannel(position).id; }
    std::string digitalChannelPhase(const int position) const { return digitalChannel(position).phase; }
    std::string digitalChannelCcbm(const int position) const { return digitalChannel(position).ccbm; }
    int digitalChannelNormalState(const int position) const { return digitalChannel(position).normal_state; }

    int sampleRateCount() const { return static_cast<int>(reader_.getCfg().sample_rates.size()); }
    double sampleRateFrequency(const int position) const { return sampleRate(position).samples_per_second; }
    long long sampleRateEndSample(const int position) const {
        return static_cast<long long>(sampleRate(position).end_sample);
    }

private:
    static long long checkedUnsignedLongLong(const std::uint64_t value, const char* description) {
        if (value > static_cast<std::uint64_t>(std::numeric_limits<long long>::max())) {
            throw std::overflow_error(std::string(description) + " exceeds Java long range");
        }
        return static_cast<long long>(value);
    }

    const comtrade::AnalogChannel& analogChannel(const int position) const {
        const auto& channels = reader_.getCfg().analog_channels;
        return channels.at(comtrade_java_detail::checkedIndex(
            position, channels.size(), "analog channel position"));
    }
    const comtrade::DigitalChannel& digitalChannel(const int position) const {
        const auto& channels = reader_.getCfg().digital_channels;
        return channels.at(comtrade_java_detail::checkedIndex(
            position, channels.size(), "digital channel position"));
    }
    const comtrade::SampleRate& sampleRate(const int position) const {
        const auto& rates = reader_.getCfg().sample_rates;
        return rates.at(comtrade_java_detail::checkedIndex(
            position, rates.size(), "sample rate position"));
    }

    comtrade::StreamReader reader_;
};

class ComtradeNativeStreamWriter {
public:
    explicit ComtradeNativeStreamWriter(const ComtradeNativeRecord& record)
        : writer_(record.nativeCfg()), cfg_(record.nativeCfg()) {}

    bool open(const std::string& dat_path) { return writer_.open(dat_path); }

    void pushRow(const long long timestamp_us,
                 const std::vector<double>& analog_values,
                 const std::vector<int>& digital_values) {
        if (timestamp_us < 0 || static_cast<unsigned long long>(timestamp_us) >
                                    std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("timestamp must fit in an unsigned 32-bit microsecond offset");
        }
        comtrade_java_detail::validateSampleValues(cfg_, analog_values, digital_values);
        writer_.pushRow(static_cast<std::uint32_t>(timestamp_us), analog_values,
                        comtrade_java_detail::toDigitalValues(digital_values));
    }

    void close() { writer_.close(); }

private:
    comtrade::StreamWriter writer_;
    comtrade::CfgData cfg_;
};

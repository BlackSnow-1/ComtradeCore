#include <array>
#include <comtrade/ai.hpp>
#include <comtrade/ai_statistics.hpp>
#include <comtrade/stream_reader.hpp>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <openssl/evp.h>
#include <sstream>

namespace comtrade::ai {
namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
std::string hash(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Cannot read input file");
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    require(ctx && EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) == 1, "Cannot initialize SHA256");
    std::array<char, 65536> block{};
    while (file.read(block.data(), block.size()) || file.gcount())
        require(EVP_DigestUpdate(ctx.get(), block.data(), size_t(file.gcount())) == 1,
                "SHA256 update failed");
    require(file.eof(), "Input read failed");
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned length = 0;
    require(EVP_DigestFinal_ex(ctx.get(), digest, &length) == 1, "SHA256 failed");
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned i = 0; i < length; ++i)
        out << std::setw(2) << unsigned(digest[i]);
    return out.str();
}
Json statsJson(const std::vector<Statistics> &stats) {
    auto out = Json::array();
    for (const auto &s : stats) {
        Json value{{"count", s.count}};
        if (s.count) {
            value["min"] = s.minimum;
            value["max"] = s.maximum;
            value["mean"] = s.mean;
            value["rms"] = s.rms();
            value["minTimeNs"] = std::to_string(s.minTime);
            value["maxTimeNs"] = std::to_string(s.maxTime);
        }
        out.push_back(value);
    }
    return out;
}
struct Window {
    std::int64_t first, last;
    std::vector<Statistics> stats;
};
std::int64_t physicalRows(const std::filesystem::path &path, const CfgData &cfg) {
    if (cfg.data_type != DataType::ASCII) {
        auto width = 8 + std::uint64_t(cfg.analog_count) * (cfg.data_type == DataType::BINARY ? 2 : 4) +
                     2 * ((std::uint64_t(cfg.digital_count) + 15) / 16);
        auto size = std::filesystem::file_size(path);
        require(size % width == 0, "Partial binary DAT row");
        return size / width;
    }
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Cannot open DAT");
    std::int64_t lines = 0;
    char last = '\n';
    std::array<char, 65536> block{};
    while (file.read(block.data(), block.size()) || file.gcount()) {
        for (std::streamsize i = 0; i < file.gcount(); ++i)
            if (block[size_t(i)] == '\n')
                ++lines;
        last = block[size_t(file.gcount() - 1)];
    }
    require(file.eof(), "DAT read failed");
    return lines + (last != '\n');
}
} // namespace
Json summarize(const std::filesystem::path &cfgPath, const std::filesystem::path &datPath,
               const Config &config) {
    config.validate();
    auto cfgHash = hash(cfgPath), datHash = hash(datPath);
    StreamReader reader(cfgPath.u8string());
    const auto &cfg = reader.getCfg();
    require(cfg.analog_count >= 0 && cfg.digital_count >= 0 &&
                std::int64_t(cfg.analog_count) + cfg.digital_count <= config.maxChannels,
            "Channel limit exceeded");
    auto rows = physicalRows(datPath, cfg);
    if (config.fullWaveform)
        require(rows <= config.maxWaveformSamples, "Full waveform sample limit exceeded");
    std::vector<Statistics> all(cfg.analog_count), before(cfg.analog_count), after(cfg.analog_count);
    std::vector<Window> windows;
    std::vector<std::int64_t> transitions(cfg.digital_count);
    std::vector<bool> initial, previous;
    Json events = Json::array(), waveform = Json::array();
    std::int64_t samples = 0, badTimes = 0, badIndices = 0, nonFinite = 0, omitted = 0, waveformBytes = 0;
    std::int64_t firstTime = 0, lastTime = 0, width = std::int64_t(config.windowMs) * 1000000;
    uint32_t lastIndex = 0;
    const auto trigger = (cfg.trigger_time - cfg.start_time).count();
    reader.processDatStream(datPath.u8string(), [&](const SampleRow &row) {
        auto time = row.time_offset.count();
        if (!samples) {
            firstTime = time;
            initial = row.digital_values;
        } else {
            if (time <= lastTime)
                ++badTimes;
            if (std::uint64_t(row.index) != std::uint64_t(lastIndex) + 1)
                ++badIndices;
        }
        if (windows.empty() || (time >= windows.back().first && time - windows.back().first >= width)) {
            if (int(windows.size()) >= config.maxWindows) {
                std::vector<Window> merged;
                for (size_t i = 0; i < windows.size(); i += 2) {
                    Window w = std::move(windows[i]);
                    if (i + 1 < windows.size()) {
                        w.last = windows[i + 1].last;
                        for (size_t c = 0; c < w.stats.size(); ++c)
                            w.stats[c].merge(windows[i + 1].stats[c]);
                    }
                    merged.push_back(std::move(w));
                }
                windows = std::move(merged);
                width = width <= std::numeric_limits<std::int64_t>::max() / 2
                            ? width * 2
                            : std::numeric_limits<std::int64_t>::max();
            }
            windows.push_back({time, time, std::vector<Statistics>(cfg.analog_count)});
        }
        windows.back().last = time;
        Json values{std::int64_t(row.index), std::to_string(row.raw_timestamp), std::to_string(time)};
        for (size_t c = 0; c < all.size(); ++c) {
            double value = row.analog_values[c];
            if (!std::isfinite(value)) {
                ++nonFinite;
                require(!config.fullWaveform, "Non-finite full waveform value");
                continue;
            }
            all[c].add(value, time);
            (time < trigger ? before[c] : after[c]).add(value, time);
            windows.back().stats[c].add(value, time);
            if (config.fullWaveform)
                values.push_back(value);
        }
        for (size_t c = 0; c < transitions.size(); ++c) {
            bool value = row.digital_values[c];
            if (samples && value != previous[c]) {
                ++transitions[c];
                if (events.size() < config.maxDigitalEvents)
                    events.push_back(Json{{"channel", ("D" + std::to_string(c + 1))},
                                          {"timeNs", std::to_string(time)},
                                          {"value", value}});
                else
                    ++omitted;
            }
            if (config.fullWaveform)
                values.push_back(value);
        }
        if (config.fullWaveform) {
            require(samples < config.maxWaveformSamples, "Full waveform sample limit exceeded");
            waveformBytes += values.dump().size() + 1;
            require(waveformBytes <= config.maxRequestBytes, "Full waveform byte limit exceeded");
            waveform.push_back(values);
        }
        previous = row.digital_values;
        lastTime = time;
        lastIndex = row.index;
        ++samples;
    });
    require(samples > 0, "No valid DAT samples");
    std::int64_t expected = cfg.sample_rates.empty() ? 0 : cfg.sample_rates.back().end_sample;
    if (config.fullWaveform)
        require(samples == rows && (!expected || expected == samples), "Incomplete full waveform");
    Json channels = Json::array(), rates = Json::array(), digital = Json::array(), windowJson = Json::array(),
         columns{"sampleIndex", "rawTimestamp", "timeOffsetNs"};
    for (size_t i = 0; i < all.size(); ++i) {
        const auto &a = cfg.analog_channels[i];
        auto id = ("A" + std::to_string(i + 1));
        Json c{{"channel", id},        {"phase", a.phase},         {"unit", a.uu},
               {"primary", a.primary}, {"secondary", a.secondary}, {"primarySecondary", a.ps},
               {"skew", a.skew}};
        if (config.includeIdentifiers)
            c["name"] = a.id;
        channels.push_back(c);
        columns.push_back(id);
    }
    for (size_t i = 0; i < transitions.size(); ++i) {
        auto id = ("D" + std::to_string(i + 1));
        Json c{{"channel", id}, {"normalState", cfg.digital_channels[i].normal_state}};
        if (config.includeIdentifiers)
            c["name"] = cfg.digital_channels[i].id;
        channels.push_back(c);
        columns.push_back(id);
        digital.push_back(Json{{"channel", id},
                               {"initial", bool(initial[i])},
                               {"final", bool(previous[i])},
                               {"transitions", transitions[i]}});
    }
    for (const auto &rate : cfg.sample_rates)
        rates.push_back(Json{{"samplesPerSecond", rate.samples_per_second},
                             {"endSample", std::int64_t(rate.end_sample)}});
    for (const auto &w : windows)
        windowJson.push_back(Json{{"firstTimeNs", std::to_string(w.first)},
                                  {"lastTimeNs", std::to_string(w.last)},
                                  {"analog", statsJson(w.stats)}});
    Json warnings{
        "RMS is sample-weighted and includes DC; windows are summaries, not identified electrical cycles.",
        "Trigger is not necessarily fault inception. Channel skew is not compensated.",
        "No phasor, frequency, harmonic, protection-setting or fault-distance calculation. Review original "
        "waveforms.",
        "DAT timestamps use TIMEMULT; zero timestamps are not reconstructed from sample rates."};
    if (samples != rows || (expected && samples != expected))
        warnings.push_back("DAT/CFG count mismatch or skipped malformed rows.");
    if (badTimes || badIndices)
        warnings.push_back("Non-increasing time or discontinuous sample sequence; timing is unreliable.");
    if (nonFinite)
        warnings.push_back("Non-finite analog values excluded from statistics.");
    if (omitted)
        warnings.push_back("Digital event list truncated; transition totals are complete.");
    if (trigger < firstTime || trigger > lastTime)
        warnings.push_back("Trigger lies outside sampled time range.");
    Json result{{"schemaVersion", 2},
                {"samples", samples},
                {"physicalRows", rows},
                {"cfgExpectedSamples", expected},
                {"cfgSha256", cfgHash},
                {"datSha256", datHash},
                {"waveformMode", config.fullWaveform ? "full" : "summary"},
                {"dataType", DataTypeUtils::ToString(cfg.data_type)},
                {"nominalFrequencyHz", cfg.line_frequency},
                {"startTimeNs", std::to_string(cfg.start_time.time_since_epoch().count())},
                {"triggerOffsetNs", std::to_string(trigger)},
                {"timeCode", cfg.time_code},
                {"localCode", cfg.local_code},
                {"timeQuality", cfg.time_quality_code},
                {"firstTimeNs", std::to_string(firstTime)},
                {"lastTimeNs", std::to_string(lastTime)},
                {"channels", channels},
                {"sampleRates", rates},
                {"analog", statsJson(all)},
                {"beforeTrigger", statsJson(before)},
                {"fromTrigger", statsJson(after)},
                {"windows", windowJson},
                {"digital", digital},
                {"digitalEvents", events},
                {"digitalEventsOmitted", omitted},
                {"nonFiniteValues", nonFinite},
                {"nonIncreasingTimes", badTimes},
                {"indexDiscontinuities", badIndices},
                {"warnings", warnings}};
    if (config.includeIdentifiers) {
        result["station"] = cfg.station_name;
        result["device"] = cfg.rec_dev_id;
    }
    if (config.fullWaveform)
        result["waveform"] = Json{{"columns", columns}, {"rows", waveform}};
    require(hash(cfgPath) == cfgHash && hash(datPath) == datHash, "Input changed during analysis");
    return result;
}

} // namespace comtrade::ai

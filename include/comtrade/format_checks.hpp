#pragma once

#include "binary_io.hpp"
#include "utils.hpp"

#include <charconv>
#include <locale>
#include <string_view>

namespace comtrade::detail {

inline void checkTextLine(Warnings& warnings, const char* newline_code, const char* encoding_code,
                          std::string_view line, bool terminated, uint64_t row) {
    if (!terminated || line.empty() || line.back() != '\r')
        addWarning(warnings, newline_code, "Expected CRLF at the end of the line", row);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    for (unsigned char byte : line) {
        if (byte < 32 || byte > 126) {
            addWarning(warnings, encoding_code, "Expected printable ASCII bytes", row);
            break;
        }
    }
}

inline bool unsignedField(std::string_view text, size_t width, uint64_t& value) {
    if (text.empty() || text.size() > width) return false;
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

inline bool realField(const std::string& text, size_t width, double& value) {
    if (text.empty() || text.size() > width) return false;
    int64_t integer = 0;
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, integer);
    if (parsed.ec == std::errc{} && parsed.ptr == end) {
        value = static_cast<double>(integer);
        return true;
    }
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    return bool(input >> value) && input.eof() && std::isfinite(value);
}

struct IeeeCfgLayout {
    StandardVersion version = StandardVersion::V1991;
    DataType type = DataType::ASCII;
    size_t analog = 0, digital = 0;
    uint64_t samples = 0;
    bool timestamp_required = false;
    bool usable = false;
    std::vector<std::pair<double, double>> ranges;
};

inline IeeeCfgLayout checkCfgLayout(const std::vector<std::string>& lines, Warnings& warnings) {
    IeeeCfgLayout layout;
    size_t row = 0;
    auto warn = [&](const char* code, const std::string& message) { addWarning(warnings, code, message, row); };
    auto read = [&](std::string_view name, size_t count, size_t minimum = 0) {
        ++row;
        auto fields = row <= lines.size() ? utils::split(lines[row-1]) : std::vector<std::string>{};
        if (row > lines.size() || fields.size() < (minimum ? minimum : count) || fields.size() > count)
            warn("ieee.cfg.fields", std::string(name) + ": expected " + std::to_string(count) + " fields");
        fields.resize(count);
        return fields;
    };
    const auto id = read("Station/device/revision", 3, 2);
    layout.version = utils::determineVersion(id[2]);
    const bool legacy = layout.version == StandardVersion::V1991;
    const bool modern = layout.version == StandardVersion::V2013;
    if (!id[2].empty() && id[2] != "1991" && id[2] != "1999" && id[2] != "2013")
        warn("ieee.cfg.revision", "Unsupported revision: " + id[2]);
    if (id[0].size() > 64 || id[1].size() > 64)
        warn("ieee.cfg.identity", "Station and device identifiers must fit in 64 characters");

    const auto counts = read("Channel counts", 3);
    auto channelCount = [&](const std::string& field, char suffix, uint64_t& value) {
        auto number = std::string_view(field);
        if (!number.empty() && (number.back() == suffix || number.back() == std::tolower(suffix))) {
            if (number.back() != suffix) warn("ieee.cfg.channel_suffix", "Expected uppercase A/D suffix");
            number.remove_suffix(1);
        } else warn("ieee.cfg.channel_suffix", "Missing A/D channel count suffix");
        return unsignedField(number, 6, value);
    };
    uint64_t a = 0, d = 0, total = 0;
    if (!channelCount(counts[1], 'A', a) || !channelCount(counts[2], 'D', d) || a+d > 999999) {
        warn("ieee.cfg.channel_count", "Invalid analog/digital channel count");
        return layout;
    }
    if (!unsignedField(counts[0], 7, total) || total == 0 || total != a+d || (legacy && total > 99))
        warn("ieee.cfg.channel_count", "Invalid total channel count; expected A+D = " + std::to_string(a+d));
    if (a+d > lines.size()-std::min(row, lines.size())) {
        warn("ieee.cfg.channel_rows", "Fewer CFG rows than declared channels");
        return layout;
    }
    layout.analog = static_cast<size_t>(a);
    layout.digital = static_cast<size_t>(d);
    for (uint64_t i = 0; i < a+d; ++i) {
        const bool analog = i < a;
        const auto fields = read(analog ? "Analog channel" : "Status channel", analog ? (legacy ? 10 : 13) : (legacy ? 3 : 5));
        uint64_t index = 0;
        const auto expected = analog ? i+1 : i-a+1;
        if (!unsignedField(fields[0], 6, index) || index != expected)
            warn("ieee.cfg.channel_index", std::string(analog ? "Analog" : "Status") +
                 " channel index should be " + std::to_string(expected) + "; found '" + fields[0] + "'");
        if ((modern && fields[1].empty()) || fields[1].size() > (modern ? 128u : 64u))
            warn("ieee.cfg.channel_id", "Invalid channel identifier length");
        if (analog || !legacy) {
            if (fields[2].size() > 2 || fields[3].size() > 64)
                warn("ieee.cfg.channel_text", "Phase or monitored component exceeds its field length");
        }
        if (!analog) {
            const auto& state = fields[legacy ? 2 : 4];
            if (state != "0" && state != "1")
                warn("ieee.cfg.normal_state", "Status normal state must be 0 or 1");
            continue;
        }
        if (fields[4].empty() || fields[4].size() > 32)
            warn("ieee.cfg.units", "Expected analog channel units (1 to 32 characters)");
        for (size_t col : {5u, 6u, 7u, 10u, 11u}) {
            if (col >= fields.size() || (col == 7 && !modern && fields[col].empty())) continue;
            double value = 0;
            if (!realField(fields[col], 32, value))
                warn("ieee.cfg.analog_number", "Invalid analog channel field " + std::to_string(col+1));
        }
        double min = 0, max = 0;
        const size_t width = modern ? 13 : 6;
        if (!realField(fields[8], width, min) || !realField(fields[9], width, max) ||
            min > max || (modern && (min < -3.4028235e38 || max > 3.4028235e38)) ||
            (!modern && (std::trunc(min) != min || std::trunc(max) != max || min < -99999 || max > 99999))) {
            warn("ieee.cfg.analog_range", "Invalid analog minimum/maximum range");
            min = -std::numeric_limits<double>::infinity();
            max = std::numeric_limits<double>::infinity();
        }
        layout.ranges.emplace_back(min, max);
        if (!legacy && (fields[12].size() != 1 || std::string_view("PpSs").find(fields[12]) == std::string_view::npos))
            warn("ieee.cfg.scaling", "Expected P or S scaling identifier");
    }

    const auto frequency = read("Nominal frequency", 1);
    double value = 0;
    if ((!modern || !frequency[0].empty()) && (!realField(frequency[0], 32, value) || value <= 0))
        warn("ieee.cfg.frequency", "Expected positive nominal frequency");
    const auto rates = read("Sampling segment count", 1);
    uint64_t segments = 0;
    if (!unsignedField(rates[0], 3, segments)) {
        warn("ieee.cfg.nrates", "Invalid number of sampling rates");
        return layout;
    }
    if (legacy && segments == 0)
        warn("ieee.cfg.nrates", "Revision 1991 requires at least one sampling rate");
    layout.timestamp_required = legacy || segments == 0;
    for (uint64_t i = 0; i < std::max<uint64_t>(1, segments); ++i) {
        const auto fields = read("Sampling rate/end sample", 2);
        uint64_t end = 0;
        if (!realField(fields[0], 32, value) || (segments ? value <= 0 : value != 0))
            warn("ieee.cfg.samp", segments ? "Expected a positive sampling rate" : "nrates=0 requires samp=0");
        if (!unsignedField(fields[1], 10, end) || end <= layout.samples)
            warn("ieee.cfg.endsamp", "Expected a positive, increasing end sample number");
        else layout.samples = end;
    }
    for (const char* name : {"Start date/time", "Trigger date/time"}) {
        const auto fields = read(name, 2);
        static const std::regex date(R"(([0-9]{1,2})/([0-9]{1,2})/([0-9]{4}))");
        static const std::regex legacy_date(R"(([0-9]{1,2})/([0-9]{1,2})/([0-9]{2}))");
        static const std::regex clock(R"([0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{6,9})");
        std::smatch parts;
        if (!std::regex_match(fields[0], parts, legacy ? legacy_date : date) ||
            !std::regex_match(fields[1], clock) || (!modern && fields[1].size() != 15)) {
            warn("ieee.cfg.datetime", std::string("Invalid ") + name);
            continue;
        }
        const int day = std::stoi(parts[legacy ? 2 : 1]), month = std::stoi(parts[legacy ? 1 : 2]);
        const int year = std::stoi(parts[3]) + (legacy ? 1900 : 0);
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        static constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        if (year < 1900 || month < 1 || month > 12 || day < 1 ||
            day > days[month-1] + (month == 2 && leap) ||
            fields[1].substr(0, 2) > "23" || fields[1].substr(3, 2) > "59" || fields[1].substr(6, 2) > "59")
            warn("ieee.cfg.datetime", std::string("Invalid ") + name);
    }
    const auto type = read("DAT encoding", 1);
    try { layout.type = DataTypeUtils::FromString(type[0]); }
    catch (const std::invalid_argument&) {
        warn("ieee.cfg.file_type", "Expected a supported DAT encoding");
        return layout;
    }
    if (!modern && (layout.type == DataType::BINARY32 || layout.type == DataType::FLOAT32))
        warn("ieee.cfg.file_type", "BINARY32 and FLOAT32 require revision 2013");
    if (layout.type != DataType::ASCII && layout.samples > UINT32_MAX)
        warn("ieee.cfg.endsamp", "Binary DAT sample number cannot exceed 4294967295");
    if (!legacy) {
        const auto mult = read("Timestamp multiplier", 1);
        if (!realField(mult[0], 32, value) || value <= 0)
            warn("ieee.cfg.timemult", "Expected a positive timestamp multiplier");
    }
    if (modern) {
        const auto zones = read("Time code/local code", 2);
        static const std::regex zone(R"(0|[+-][0-9]{1,2}(h[0-5][0-9])?)");
        int minutes = 0;
        auto validZone = [&](const std::string& code) {
            return std::regex_match(code, zone) && utils::parseUtcOffset(code, minutes);
        };
        if (!validZone(zones[0]) ||
            !(validZone(zones[1]) || (zones[0] == "0" && zones[1] == "x")) ||
            (zones[0] != "0" && zones[0] != zones[1]))
            warn("ieee.cfg.zone", "Invalid time code/local code pair");
        const auto quality = read("Time quality/leap second", 2);
        if (quality[0].size() != 1 || std::string_view("0123456789ABFabf").find(quality[0]) == std::string_view::npos ||
            quality[1].size() != 1 || quality[1][0] < '0' || quality[1][0] > '3')
            warn("ieee.cfg.time_quality", "Invalid time quality or leap-second code");
    }
    if (row < lines.size()) addWarning(warnings, "ieee.cfg.trailing", "Unexpected CFG rows", row+1);
    layout.usable = true;
    return layout;
}

class DatFormatCheck {
public:
    DatFormatCheck(const IeeeCfgLayout& layout, Warnings& warnings) : layout_(layout), warnings_(warnings) {}

    void ascii(const std::string& line, bool terminated, const std::vector<std::string>& fields, uint64_t row) {
        checkTextLine(warnings_, "ieee.dat.crlf", "ieee.dat.ascii", line, terminated, row);
        asciiFields(fields, row);
    }

    void asciiFields(const std::vector<std::string>& fields, uint64_t row) {
        const auto expected = 2+layout_.analog+layout_.digital;
        if (fields.size() != expected)
            addWarning(warnings_, "ieee.dat.columns", "Expected " + std::to_string(expected) +
                       " columns; found " + std::to_string(fields.size()), row);
        uint64_t index = 0, timestamp = 0;
        const bool modern = layout_.version == StandardVersion::V2013;
        if (fields.empty() || !unsignedField(fields[0], 10, index) || index == 0)
            addWarning(warnings_, "ieee.dat.index_field", "Invalid sample number", row);
        const bool time_present = fields.size() > 1 && unsignedField(fields[1], modern ? 13 : 10, timestamp);
        if (!time_present && fields.size() > 1 && !fields[1].empty())
            addWarning(warnings_, "ieee.dat.timestamp_field", "Invalid timestamp field", row);
        header(row, index, time_present, timestamp);
        for (size_t col = 2; col < std::min(fields.size(), expected); ++col) {
            const auto& field = fields[col];
            if (col >= 2+layout_.analog) {
                if (field != "0" && field != "1") addWarning(warnings_, "ieee.dat.status",
                    "Status channel " + std::to_string(col-1-layout_.analog) + " must be 0 or 1", row);
                continue;
            }
            if (field.empty() && modern) continue;
            if (layout_.version == StandardVersion::V1991 && field == "999999") continue;
            double value = 0;
            if (!realField(field, modern ? 13 : 6, value) ||
                (modern && std::abs(value) > 3.4028235e38) ||
                (!modern && (std::trunc(value) != value || value < -99999 ||
                 value > (layout_.version == StandardVersion::V1991 ? 999998 : 99999) ||
                 field.find_first_not_of("+-0123456789") != std::string::npos)))
                addWarning(warnings_, "ieee.dat.analog", "Invalid analog value on channel " + std::to_string(col-1), row);
            else if (layout_.version != StandardVersion::V1999 || value != 99999) analog(col-2, value, row);
        }
    }

    void binary(const char* cursor, uint64_t row) {
        const auto index = readUint32LittleEndian(cursor);
        const auto timestamp = readUint32LittleEndian(cursor);
        header(row, index, timestamp != UINT32_MAX, timestamp);
        for (size_t i = 0; i < layout_.analog; ++i) {
            double value = 0;
            double missing = 0;
            switch (layout_.type) {
                case DataType::BINARY:
                    value = readInt16LittleEndian(cursor);
                    missing = layout_.version == StandardVersion::V1991 ? -1 : -32768;
                    break;
                case DataType::BINARY32: value = readInt32LittleEndian(cursor); missing = INT32_MIN; break;
                case DataType::FLOAT32: value = readFloat32LittleEndian(cursor); missing = -std::numeric_limits<float>::max(); break;
                default: return;
            }
            if (!std::isfinite(value))
                addWarning(warnings_, "ieee.dat.float", "Non-finite FLOAT32 on channel " + std::to_string(i+1), row);
            else if (value != missing) analog(i, value, row);
        }
        const auto remaining = layout_.digital % 16;
        if (remaining && layout_.version == StandardVersion::V2013) {
            cursor += 2*(layout_.digital/16);
            if (readUint16LittleEndian(cursor) >> remaining)
                addWarning(warnings_, "ieee.dat.padding", "Unused status bits must be zero", row);
        }
    }

    void finish(uint64_t rows, bool eof_marker = false) {
        if (layout_.samples && rows != layout_.samples)
            addWarning(warnings_, "ieee.dat.sample_count", "CFG endsamp=" + std::to_string(layout_.samples) +
                       "; DAT complete rows=" + std::to_string(rows), rows < layout_.samples ? rows+1 : layout_.samples+1);
        if (!rows) addWarning(warnings_, "ieee.dat.empty", "DAT contains no sample records");
        if (layout_.type == DataType::ASCII && layout_.version != StandardVersion::V1999 && !eof_marker)
            addWarning(warnings_, "ieee.dat.eof", "Expected ASCII EOF marker 0x1A after the final CRLF", rows+1);
    }

private:
    void header(uint64_t row, uint64_t index, bool has_time, uint64_t time) {
        if (index != row)
            addWarning(warnings_, "ieee.dat.sequence", "Expected sample " + std::to_string(row) +
                       "; found " + std::to_string(index), row);
        if (!has_time && layout_.timestamp_required)
            addWarning(warnings_, "ieee.dat.timestamp_required", "CFG revision or rate layout requires a timestamp", row);
        if (has_time && ((row == 1 && time != 0) || (have_time_ && time < previous_time_) ||
                         (layout_.timestamp_required && have_time_ && time == previous_time_)))
            addWarning(warnings_, "ieee.dat.time_order", "Expected a zero start offset and increasing sample time", row);
        if (has_time) { previous_time_ = time; have_time_ = true; }
    }
    void analog(size_t i, double value, uint64_t row) {
        if (i < layout_.ranges.size() && (value < layout_.ranges[i].first || value > layout_.ranges[i].second))
            addWarning(warnings_, "ieee.dat.analog_range", "Value outside CFG min/max on analog channel " +
                       std::to_string(i+1), row);
    }

    const IeeeCfgLayout& layout_;
    Warnings& warnings_;
    uint64_t previous_time_ = 0;
    bool have_time_ = false;
};

} // namespace comtrade::detail

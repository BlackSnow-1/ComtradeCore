/**
 * @file stream_reader.hpp
 * @brief 以恒定内存逐行解析 ASCII 和二进制 DAT，并结合 CFG 输出工程量和绝对时间。
 */
#pragma once

#include "binary_io.hpp"
#include "cfg_io.hpp"
#include "cff_io.hpp"
#include "text_encoding.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace comtrade {

// 回调看到的单个采样点。raw_timestamp 保留 DAT 原值，其余时间字段为换算结果。
struct SampleRow {
    uint32_t index = 0;
    // ASCII DAT files may use values beyond the 32-bit binary timestamp field.
    uint64_t raw_timestamp = 0;
    uint64_t timestamp_us = 0;
    std::chrono::nanoseconds time_offset{};
    TimePoint absolute_time{};
    bool timestamp_reconstructed = false;
    std::vector<bool> analog_missing;
    std::vector<double> analog_values;
    std::vector<bool> digital_values;
    // 2013 版允许 nrates=0 后跟任意数量的采样段，每段可以有不同的采样率；这两个字段
    // 让调用方在流式回调里直接知道"当前点属于哪一段、这段标称多少赫兹"，不必自己按
    // cfg().sample_rates 的 end_sample 做二分查找。segment_index 是 sample_rates 的下标，
    // 从 0 开始；sample_rates 为空（无采样段信息）时两个字段都保持默认值 0。
    std::size_t segment_index = 0;
    double segment_sample_rate = 0.0;
};

// StreamReader 在构造时固定一份 CFG 快照，之后可用它重复流式处理同格式 DAT。
class StreamReader {
public:
    explicit StreamReader(const std::string& path, ReadOptions options = {}) : options_(options) {
        std::ifstream input(path, std::ios::binary);
        std::string first; std::getline(input, first); detail::stripUtf8Bom(first);
        if (first.find("---") != std::string::npos) {
            input.clear(); input.seekg(0);
            detail::CffSections cff;
            if (!detail::readCff(input, cff)) throw std::runtime_error("Invalid CFF: " + path);
            cfg_ = std::move(cff.cfg); hdr_ = std::move(cff.hdr); inf_ = std::move(cff.inf);
            cff_path_ = path; cff_offset_ = cff.dat_offset;
            cff_bytes_ = cff.dat_bytes; cff_bounded_ = cff.has_byte_count;
        } else if (!detail::parseCfgFile(path, cfg_)) throw std::runtime_error("Invalid CFG: " + path);
        warnings_ = cfg_.warnings;
    }
    explicit StreamReader(CfgData cfg, ReadOptions options = {}) : cfg_(std::move(cfg)), options_(options), warnings_(cfg_.warnings) {}
    const DatTail& getDatTail() const noexcept { return dat_tail_; }
    // Invoked synchronously for every tail chunk, even beyond the retention limit.
    // The caller must copy bytes it needs after the callback returns; exceptions propagate.
    void setDatTailHandler(std::function<void(const char*, size_t, uint64_t)> handler) {
        tail_handler_ = std::move(handler);
    }
    uint64_t getPhysicalRowCount() const noexcept { return physical_rows_; }
    const Warnings& getWarnings() const noexcept { return warnings_; }
    const std::string& getHeader() const noexcept { return hdr_; }
    const std::string& getInformation() const noexcept { return inf_; }
    const ReadOptions& getReadOptions() const noexcept { return options_; }
    void setReadOptions(ReadOptions options) { options_ = options; }
    bool isCff() const noexcept { return !cff_path_.empty(); }
    size_t processCffStream(const std::function<void(const SampleRow&)>& callback) const {
        if (!isCff()) throw std::logic_error("Reader was not opened from CFF");
        std::ifstream input(cff_path_, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open CFF");
        input.seekg(0, std::ios::end);
        const auto available = static_cast<uint64_t>(input.tellg() - cff_offset_);
        input.seekg(cff_offset_);
        auto count = processStream(input, callback, cff_bounded_ ? std::min(available, cff_bytes_) : available);
        if (cff_bounded_ && available != cff_bytes_)
            addWarning(warnings_, "cff.byte_count", "Declared CFF DAT byte count differs from available bytes");
        return count;
    }

    [[nodiscard]] const CfgData& getCfg() const noexcept {
        return cfg_;
    }

    size_t processDatStream(const std::string& dat_filepath,
                            const std::function<void(const SampleRow&)>& on_row_parsed) const {
        std::ifstream input(dat_filepath, std::ios::binary);
        if (!input) { dat_tail_ = {}; physical_rows_ = 0; warnings_ = cfg_.warnings; addWarning(warnings_, "dat.open", "Cannot open DAT"); return 0; }
        return processStream(input, on_row_parsed);
    }
    size_t processStream(std::istream& input, const std::function<void(const SampleRow&)>& callback,
                         uint64_t byte_limit = std::numeric_limits<uint64_t>::max()) const {
        warnings_ = cfg_.warnings; physical_rows_ = 0; dat_tail_ = {};
        if (!std::isfinite(cfg_.time_multiplier) || cfg_.time_multiplier <= 0 ||
            cfg_.analog_count < 0 || cfg_.digital_count < 0 ||
            cfg_.analog_channels.size() != static_cast<size_t>(cfg_.analog_count) ||
            cfg_.digital_channels.size() != static_cast<size_t>(cfg_.digital_count))
            throw std::invalid_argument("Inconsistent CFG layout");
        SampleRow row;
        row.analog_values.resize(cfg_.analog_count); row.analog_missing.resize(cfg_.analog_count);
        row.digital_values.resize(cfg_.digital_count);
        uint64_t previous_index = 0; int64_t previous_time = -1;
        auto checked = [&](const SampleRow& value) {
            if (value.index != previous_index + 1) addWarning(warnings_, "dat.index", "Discontinuous sample sequence", value.index);
            if (value.time_offset.count() <= previous_time) addWarning(warnings_, "dat.time_order", "Non-increasing sample time", value.index);
            previous_index = value.index; previous_time = value.time_offset.count(); callback(value);
        };
        const auto count = cfg_.data_type == DataType::ASCII ? processAsciiDat(input, row, checked, byte_limit)
                         : processBinaryDat(input, row, checked, byte_limit);
        if (!cfg_.sample_rates.empty() && count != cfg_.sample_rates.back().end_sample)
            addWarning(warnings_, "dat.sample_count", "Parsed sample count differs from CFG");
        if (input.bad()) addWarning(warnings_, "dat.io", "I/O error while reading DAT");
        return count;
    }

private:
    uint64_t declaredSampleEnd() const noexcept {
        if (options_.dat_tail_policy == DatTailPolicy::ParseToEnd || cfg_.sample_rates.empty()) return 0;
        return cfg_.sample_rates.back().end_sample;
    }
    void readDatTail(std::istream& input, uint64_t consumed, uint64_t byte_limit) const {
        dat_tail_.offset = consumed;
        std::array<char, 65536> buffer{};
        while (consumed < byte_limit && input) {
            const auto requested = static_cast<std::streamsize>(std::min<uint64_t>(buffer.size(), byte_limit-consumed));
            input.read(buffer.data(), requested);
            const auto length = static_cast<size_t>(input.gcount());
            if (!length) break;
            const auto retained = std::min(length, options_.max_retained_tail_bytes-dat_tail_.bytes.size());
            dat_tail_.bytes.append(buffer.data(), retained);
            dat_tail_.byte_count += length;
            if (tail_handler_) tail_handler_(buffer.data(), length, consumed);
            consumed += length;
        }
        dat_tail_.truncated = dat_tail_.byte_count > dat_tail_.bytes.size();
        if (dat_tail_.byte_count)
            addWarning(warnings_, "dat.vendor_tail", "Bytes after CFG sample boundary retained as opaque DAT tail; verify CFG count if additional samples were expected");
        if (dat_tail_.truncated)
            addWarning(warnings_, "dat.vendor_tail_limit", "Tail retention limit reached; use the chunk callback to extract the complete tail");
    }

    // 按 DAT 采样序号定位采样段；跳过坏行后也保留原始段边界。DAT 严格按序
    // 流式产生，序号只会单调增加，因此 cursor 只需单向前移；整份文件的定位总代价是
    // O(采样段数)，不会随采样点数增长，保持流式处理原有的常数级内存和线性时间特征。
    void updateSegment(SampleRow& row, std::size_t& cursor, uint64_t physical_ordinal) const {
        if (cfg_.sample_rates.empty()) {
            row.segment_index = 0;
            row.segment_sample_rate = 0.0;
            return;
        }
        if (cursor > 0 && physical_ordinal <= cfg_.sample_rates[cursor-1].end_sample) cursor = 0;
        while (cursor + 1 < cfg_.sample_rates.size() && physical_ordinal > cfg_.sample_rates[cursor].end_sample)
            ++cursor;
        row.segment_index = cursor;
        row.segment_sample_rate = cfg_.sample_rates[cursor].samples_per_second;
    }

    bool populateTimeFields(SampleRow& row, bool missing = false) const {
        long double offset = static_cast<long double>(row.raw_timestamp) * cfg_.time_multiplier * 1000.0L;
        const bool reconstruct = options_.time_axis == TimeAxisMode::SampleRates ||
            (options_.time_axis == TimeAxisMode::Automatic && (missing || (cfg_.data_type != DataType::ASCII && row.raw_timestamp == 0xffffffffULL) || (row.raw_timestamp == 0 && row.index > 1)));
        row.timestamp_reconstructed = false;
        if (reconstruct) {
            long double seconds = 0; uint64_t first = 1; bool found = row.index == 1;
            for (const auto& rate : cfg_.sample_rates) {
                if (!std::isfinite(rate.samples_per_second) || rate.samples_per_second <= 0 || rate.end_sample < first) break;
                const uint64_t last = std::min<uint64_t>(row.index, rate.end_sample);
                if (last >= first) seconds += (last - first + (first == 1 ? 0 : 1)) / static_cast<long double>(rate.samples_per_second);
                if (row.index <= rate.end_sample) { found = true; break; }
                first = uint64_t(rate.end_sample) + 1;
            }
            if (found && row.index > 0) {
                offset = seconds * 1e9L; row.timestamp_reconstructed = true;
                addWarning(warnings_, "dat.time_reconstructed", "Timestamp reconstructed from sample index and rates", row.index);
            } else {
                addWarning(warnings_, "dat.time_unavailable", "Cannot reconstruct time from the declared sample rates", row.index);
                if (missing || (cfg_.data_type != DataType::ASCII && row.raw_timestamp == 0xffffffffULL) || options_.time_axis == TimeAxisMode::SampleRates) return false;
            }
        } else if (missing) { addWarning(warnings_, "dat.time_unavailable", "Missing DAT timestamp", row.index); return false; }
        if (!std::isfinite(offset) || offset < 0 || offset >= static_cast<long double>(std::numeric_limits<int64_t>::max())) {
            addWarning(warnings_, "dat.time_overflow", "Sample time exceeds nanosecond range", row.index); return false;
        }
        const auto ns = static_cast<int64_t>(std::round(offset));
        if (cfg_.start_time.time_since_epoch().count() > std::numeric_limits<int64_t>::max() - ns) {
            addWarning(warnings_, "dat.time_overflow", "Absolute sample time overflow", row.index); return false;
        }
        row.time_offset = std::chrono::nanoseconds(ns); row.timestamp_us = static_cast<uint64_t>(ns / 1000);
        row.absolute_time = cfg_.start_time + row.time_offset; return true;
    }
    double decodeAnalog(double raw, size_t channel, SampleRow& row) const {
        const bool missing = !std::isfinite(raw) ||
            (cfg_.data_type == DataType::ASCII && cfg_.version != StandardVersion::V1991 && raw == 99999) ||
            (cfg_.data_type == DataType::BINARY && raw == (cfg_.version == StandardVersion::V1991 ? -1 : -32768)) ||
            (cfg_.data_type == DataType::BINARY32 && raw == -2147483648.0) ||
            (cfg_.data_type == DataType::FLOAT32 && raw == -static_cast<double>(std::numeric_limits<float>::max()));
        row.analog_missing[channel] = missing;
        if (missing) { addWarning(warnings_, "dat.missing_analog", "Missing analog value represented as NaN", row.index); return std::numeric_limits<double>::quiet_NaN(); }
        const auto& c = cfg_.analog_channels[channel];
        const double value = raw * c.a + c.b;
        if (!std::isfinite(value)) { row.analog_missing[channel] = true; addWarning(warnings_, "dat.analog_overflow", "Analog scaling overflow represented as NaN", row.index); }
        return std::isfinite(value) ? value : std::numeric_limits<double>::quiet_NaN();
    }

    size_t processAsciiDat(std::istream& dat_file,
                           SampleRow& row_buffer,
                           const std::function<void(const SampleRow&)>& on_row_parsed, uint64_t byte_limit) const {
        size_t parsed_count = 0;
        std::size_t segment_cursor = 0;
        std::string line;
        bool first_line = true;
        uint64_t physical_row = 0, consumed = 0;
        while (consumed < byte_limit && std::getline(dat_file, line)) {
            consumed += line.size() + (dat_file.eof() ? 0 : 1);
            ++physical_row; physical_rows_ = physical_row;
            if (consumed > byte_limit) { addWarning(warnings_, "dat.partial_row", "CFF limit cuts an ASCII row", physical_row); break; }
            if (first_line) {
                detail::stripUtf8Bom(line);
                first_line = false;
            }
            const auto tokens = utils::split(line);
            const auto expected_tokens = static_cast<std::size_t>(2 + cfg_.analog_count + cfg_.digital_count);
            if (tokens.size() < expected_tokens) { addWarning(warnings_, "dat.bad_row", "Short DAT row skipped", physical_row); continue; }
            if (tokens.size() > expected_tokens) addWarning(warnings_, "dat.extra_columns", "Extra DAT columns ignored", physical_row);

            try {
                std::size_t index_characters = 0;
                std::size_t timestamp_characters = 0;
                const auto parsed_index = std::stoull(tokens[0], &index_characters);
                const bool missing_time = tokens[1].empty();
                const auto parsed_timestamp = missing_time ? 0ULL : std::stoull(tokens[1], &timestamp_characters);
                if (tokens[0].empty() || tokens[0].front() == '-' || (!missing_time && tokens[1].front() == '-') ||
                    index_characters != tokens[0].size() || timestamp_characters != tokens[1].size() ||
                    parsed_index == 0 || parsed_index > std::numeric_limits<uint32_t>::max()) {
                    throw std::invalid_argument("Invalid index/timestamp");
                }
                row_buffer.index = static_cast<uint32_t>(parsed_index);
                row_buffer.raw_timestamp = static_cast<uint64_t>(parsed_timestamp);
                if (!populateTimeFields(row_buffer, missing_time)) continue;

                for (int i = 0; i < cfg_.analog_count; ++i) {
                    const auto& text = tokens[static_cast<size_t>(2+i)];
                    size_t used = 0;
                    const double raw_value = text.empty() ? std::numeric_limits<double>::quiet_NaN() : std::stod(text, &used);
                    if (used != text.size()) throw std::invalid_argument("Invalid analog token");
                    row_buffer.analog_values[i] = decodeAnalog(raw_value, i, row_buffer);
                }

                for (int i = 0; i < cfg_.digital_count; ++i) {
                    const auto token_index = static_cast<std::size_t>(2 + cfg_.analog_count + i);
                    size_t used = 0; const auto value = std::stoll(tokens[token_index], &used);
                    if (used != tokens[token_index].size()) throw std::invalid_argument("Invalid digital token");
                    if (value != 0 && value != 1) addWarning(warnings_, "dat.digital_state", "Nonboolean digital value interpreted as nonzero", physical_row);
                    row_buffer.digital_values[i] = value != 0;
                }
            } catch (const std::exception&) {
                addWarning(warnings_, "dat.bad_row", "Malformed DAT row skipped", physical_row);
                continue;
            }

            updateSegment(row_buffer, segment_cursor, row_buffer.index);
            on_row_parsed(row_buffer);
            ++parsed_count;
            if (declaredSampleEnd() && row_buffer.index == declaredSampleEnd()) {
                readDatTail(dat_file, consumed, byte_limit);
                break;
            }
        }

        return parsed_count;
    }

    size_t processBinaryDat(std::istream& dat_file,
                            SampleRow& row_buffer,
                            const std::function<void(const SampleRow&)>& on_row_parsed, uint64_t byte_limit) const {
        const auto analog_count = static_cast<std::size_t>(cfg_.analog_count);
        const auto digital_count = static_cast<std::size_t>(cfg_.digital_count);
        const auto row_size = detail::binaryRowSize(cfg_.data_type, analog_count, digital_count);
        std::vector<char> encoded_row(row_size);

        size_t parsed_count = 0;
        std::size_t segment_cursor = 0;
        uint64_t consumed = 0;
        while (byte_limit - consumed >= row_size && dat_file.read(encoded_row.data(), static_cast<std::streamsize>(encoded_row.size()))) {
            consumed += row_size; ++physical_rows_;
            const char* cursor = encoded_row.data();
            row_buffer.index = detail::readUint32LittleEndian(cursor);
            row_buffer.raw_timestamp = detail::readUint32LittleEndian(cursor);
            if (!populateTimeFields(row_buffer)) {
                if (declaredSampleEnd() && physical_rows_ == declaredSampleEnd()) {
                    readDatTail(dat_file, consumed, byte_limit); return parsed_count;
                }
                continue;
            }

            // BINARY、BINARY32 和 FLOAT32 仅改变模拟量原始字段宽度；工程量换算规则一致。
            for (std::size_t i = 0; i < analog_count; ++i) {
                double raw_value = 0.0;
                if (cfg_.data_type == DataType::BINARY) {
                    raw_value = static_cast<double>(detail::readInt16LittleEndian(cursor));
                } else if (cfg_.data_type == DataType::BINARY32) {
                    raw_value = static_cast<double>(detail::readInt32LittleEndian(cursor));
                } else {
                    raw_value = static_cast<double>(detail::readFloat32LittleEndian(cursor));
                }

                row_buffer.analog_values[i] = decodeAnalog(raw_value, i, row_buffer);
            }

            // 数字量按每 16 路一个 uint16 word 打包，通道 0 对应最低有效位。
            for (std::size_t word_index = 0;
                 word_index < detail::digitalWordCount(digital_count);
                 ++word_index) {
                const auto word = detail::readUint16LittleEndian(cursor);
                for (std::size_t bit = 0; bit < 16U; ++bit) {
                    const auto channel_index = word_index * 16U + bit;
                    if (channel_index >= digital_count) break;
                    row_buffer.digital_values[channel_index] = (word & (std::uint16_t{1} << bit)) != 0U;
                }
            }

            updateSegment(row_buffer, segment_cursor, row_buffer.index);
            on_row_parsed(row_buffer);
            ++parsed_count;
            if (declaredSampleEnd() && physical_rows_ == declaredSampleEnd()) {
                readDatTail(dat_file, consumed, byte_limit); return parsed_count;
            }
        }

        if ((dat_file.gcount() > 0 && static_cast<size_t>(dat_file.gcount()) < row_size) ||
            (byte_limit != std::numeric_limits<uint64_t>::max() && consumed < byte_limit))
            addWarning(warnings_, "dat.partial_row", "Incomplete final binary row ignored");
        return parsed_count;
    }

    CfgData cfg_;
    ReadOptions options_;
    mutable Warnings warnings_;
    mutable uint64_t physical_rows_ = 0;
    mutable DatTail dat_tail_;
    std::function<void(const char*, size_t, uint64_t)> tail_handler_;
    std::string cff_path_, hdr_, inf_;
    std::streamoff cff_offset_ = 0;
    uint64_t cff_bytes_ = 0;
    bool cff_bounded_ = false;
};

}  // namespace comtrade

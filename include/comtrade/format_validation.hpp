#pragma once

#include "format_checks.hpp"

#include <fstream>

namespace comtrade {

struct FormatReport {
    Warnings warnings;
    uint64_t expected_samples = 0;
    uint64_t actual_samples = 0;
    bool io_error = false;

    bool ok() const noexcept { return warnings.empty(); }
};

// A null DAT stream requests CFG-only inspection. Open file streams in binary
// mode to inspect literal delimiters and keep binary record offsets intact.
inline FormatReport checkFormat(std::istream& cfg, std::istream* dat = nullptr) {
    FormatReport report;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(cfg, line)) {
        detail::checkTextLine(report.warnings, "ieee.cfg.crlf", "ieee.cfg.ascii", line, !cfg.eof(), lines.size()+1);
        lines.push_back(std::move(line));
    }
    if (cfg.bad() || !cfg.eof()) {
        report.io_error = true;
        addWarning(report.warnings, "ieee.cfg.io", "Could not read CFG");
        return report;
    }
    const auto layout = detail::checkCfgLayout(lines, report.warnings);
    report.expected_samples = layout.samples;
    if (!dat) return report;
    if (!layout.usable) {
        addWarning(report.warnings, "ieee.dat.layout", "Cannot determine DAT layout from CFG");
        return report;
    }

    detail::DatFormatCheck check(layout, report.warnings);
    bool eof_marker = false;
    if (layout.type == DataType::ASCII) {
        while (std::getline(*dat, line)) {
            if (line == "\x1a" && dat->eof()) {
                eof_marker = true;
                break;
            }
            check.ascii(line, !dat->eof(), utils::split(line), ++report.actual_samples);
        }
    } else {
        const auto size = detail::binaryRowSize(layout.type, layout.analog, layout.digital);
        std::vector<char> record(size);
        while (dat->read(record.data(), static_cast<std::streamsize>(size)))
            check.binary(record.data(), ++report.actual_samples);
        if (dat->gcount())
            addWarning(report.warnings, "ieee.dat.partial_record",
                "Expected " + std::to_string(size) + " bytes per record; found " +
                std::to_string(dat->gcount()) + " bytes in the final record at byte offset " +
                std::to_string(report.actual_samples*size), report.actual_samples+1);
    }
    if (dat->bad() || !dat->eof()) {
        report.io_error = true;
        addWarning(report.warnings, "ieee.dat.io", "Could not read DAT");
    }
    check.finish(report.actual_samples, eof_marker);
    return report;
}

} // namespace comtrade

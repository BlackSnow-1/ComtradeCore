#pragma once
#include "cfg_io.hpp"
#include <regex>
#include <sstream>
#include <limits>

namespace comtrade::detail {
// Metadata is buffered; DAT stays in the original stream, including binary bytes.
struct CffSections {
    CfgData cfg;
    std::string hdr, inf;
    std::streamoff dat_offset = 0;
    uint64_t dat_bytes = 0;
    bool has_byte_count = false;
};
inline bool readCff(std::istream& input, CffSections& result) {
    static const std::regex marker(R"(^\s*---\s*file type:\s*(CFG|INF|HDR|DAT)(?:\s+(ASCII|BINARY|BINARY32|FLOAT32))?(?:\s*:\s*([0-9]+))?\s*---\s*$)", std::regex::icase);
    CffSections parsed;
    std::vector<std::string> lines;
    std::string section, line;
    bool have_cfg = false, first = true;
    Warnings warnings;
    while (std::getline(input, line)) {
        if (first) { stripUtf8Bom(line); first = false; }
        std::smatch match;
        if (std::regex_match(line, match, marker)) {
            section = match[1];
            std::transform(section.begin(), section.end(), section.begin(), [](unsigned char c){return char(std::toupper(c));});
            if (section == "CFG") { if (have_cfg) return false; have_cfg = true; }
            if (section == "DAT") {
                if (!have_cfg || lines.empty()) return false;
                normalizeCfgLinesToUtf8(lines);
                try {
                    if (!parseCfgLines(lines, parsed.cfg)) return false;
                    if (match[2].matched) {
                        auto encoding = DataTypeUtils::FromString(match[2]);
                        // DAT BINARY denotes all binary layouts; CFG selects the analog width.
                        if ((encoding == DataType::ASCII) != (parsed.cfg.data_type == DataType::ASCII)) return false;
                        if (encoding != DataType::BINARY && encoding != parsed.cfg.data_type) return false;
                    }
                    parsed.has_byte_count = match[3].matched;
                    if (parsed.has_byte_count) parsed.dat_bytes = std::stoull(match[3]);
                    parsed.dat_offset = input.tellg();
                    if (parsed.dat_offset < 0) { input.clear(); input.seekg(0, std::ios::end); parsed.dat_offset = input.tellg(); }
                    if (parsed.cfg.data_type != DataType::ASCII && !parsed.has_byte_count)
                        addWarning(parsed.cfg.warnings, "cff.byte_count", "Missing binary DAT byte count; reading to end of file");
                    result = std::move(parsed); return true;
                } catch (...) { return false; }
            }
            continue;
        }
        if (section == "CFG") lines.push_back(line);
        else if (section == "HDR") parsed.hdr += line + "\n";
        else if (section == "INF") parsed.inf += line + "\n";
        else if (!line.empty()) return false;
    }
    return false;
}
} // namespace comtrade::detail

#include <gtest/gtest.h>
#include <comtrade/comtrade.hpp>

#include <filesystem>
#include <sstream>

using namespace comtrade;

namespace {

std::string configuration(const std::string& type = "ASCII", const std::string& rates = "1\r\n1000,2") {
    return "Station,Device,2013\r\n2,1A,1D\r\n1,VA,A,,V,1,0,0,-100000,100000,1,1,P\r\n1,Trip,,,0\r\n"
           "50\r\n" + rates + "\r\n14/11/2026,00:00:00.000000\r\n14/11/2026,00:00:01.000000\r\n" +
           type + "\r\n1\r\n0,+8\r\n0,0\r\n";
}

const Warning* findWarning(const Warnings& warnings, const std::string& code) {
    for (const auto& warning : warnings) if (warning.code == code) return &warning;
    return nullptr;
}

TEST(FormatValidation, ValidAsciiAllowsMissingAnalogAndRecognizesEof) {
    std::istringstream cfg(configuration()), dat("1,0,99999,0\r\n2,1000,,1\r\n\x1a");
    const auto report = checkFormat(cfg, &dat);
    EXPECT_TRUE(report.ok());
    EXPECT_EQ(report.expected_samples, 2u);
    EXPECT_EQ(report.actual_samples, 2u);
}

TEST(FormatValidation, LegacyRevisionsUseTheirOwnLayoutsAndMissingMarkers) {
    const std::string legacy =
        "Station,Device\r\n2,1A,1D\r\n1,VA,A,,V,1,0,0,-100,100\r\n1,Trip,1\r\n"
        "50\r\n1\r\n1000,1\r\n11/14/96,00:00:00.000000\r\n11/14/96,00:00:01.000000\r\nASCII\r\n";
    std::istringstream cfg91(legacy), dat91("1,0,999999,0\r\n\x1a");
    EXPECT_TRUE(checkFormat(cfg91, &dat91).ok());
    std::istringstream cfg99(
        "Station,Device,1999\r\n2,1A,1D\r\n1,VA,A,,V,1,0,0,-100,100,1,1,P\r\n1,Trip,,,0\r\n"
        "50\r\n1\r\n1000,1\r\n14/11/1999,00:00:00.000000\r\n14/11/1999,00:00:01.000000\r\nASCII\r\n1\r\n");
    std::istringstream dat99("1,0,99999,0\r\n");
    EXPECT_TRUE(checkFormat(cfg99, &dat99).ok());
}

TEST(FormatValidation, ReportsIndependentCfgAndDatProblems) {
    auto text = configuration();
    text.replace(text.find("1,Trip"), 6, "2,Trip");
    text.erase(text.find('\r'), 1);
    std::istringstream cfg(text), dat("2,0,0,4\n3,1000,0");
    const auto report = checkFormat(cfg, &dat);
    EXPECT_FALSE(report.ok());
    const auto* index = findWarning(report.warnings, "ieee.cfg.channel_index");
    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->first_row, 4u);
    const auto* crlf = findWarning(report.warnings, "ieee.dat.crlf");
    ASSERT_NE(crlf, nullptr);
    EXPECT_EQ(crlf->count, 2u);
    for (const auto* code : {"ieee.cfg.crlf", "ieee.dat.sequence", "ieee.dat.columns", "ieee.dat.status", "ieee.dat.eof"})
        EXPECT_NE(findWarning(report.warnings, code), nullptr) << code;
}

TEST(FormatValidation, InspectsAllBinaryEncodingsAndDetectsTruncatedRecords) {
    for (auto type : {DataType::BINARY, DataType::BINARY32, DataType::FLOAT32}) {
        SCOPED_TRACE(DataTypeUtils::ToString(type));
        const auto size = detail::binaryRowSize(type, 1, 1);
        std::string bytes(size*2, '\0');
        for (unsigned i = 0; i < 2; ++i) {
            char* cursor = bytes.data()+i*size;
            detail::writeUint32LittleEndian(cursor, i+1);
            detail::writeUint32LittleEndian(cursor, i*1000);
        }
        const auto text = configuration(DataTypeUtils::ToString(type));
        std::istringstream good_cfg(text), good_dat(bytes);
        EXPECT_TRUE(checkFormat(good_cfg, &good_dat).ok());
        bytes[size-2] = 2;
        bytes.pop_back();
        std::istringstream bad_cfg(text), bad_dat(bytes);
        const auto report = checkFormat(bad_cfg, &bad_dat);
        EXPECT_EQ(report.actual_samples, 1u);
        for (const auto* code : {"ieee.dat.padding", "ieee.dat.partial_record", "ieee.dat.sample_count"})
            EXPECT_NE(findWarning(report.warnings, code), nullptr) << code;
    }
}

TEST(FormatValidation, VariableRateNeedsTimestampsAndNoFixedRate) {
    std::istringstream cfg(configuration("ASCII", "0\r\n0,2")), dat("1,0,0,0\r\n2,,0,0\r\n\x1a");
    const auto report = checkFormat(cfg, &dat);
    const auto* warning = findWarning(report.warnings, "ieee.dat.timestamp_required");
    ASSERT_NE(warning, nullptr);
    EXPECT_EQ(warning->first_row, 2u);
    EXPECT_EQ(report.warnings.size(), 1u);
}

TEST(FormatValidation, ReaderLogsAggregatedDiagnosticsAndRetainsRecovery) {
    std::istringstream config(configuration());
    std::vector<std::string> lines;
    for (std::string line; std::getline(config, line);) {
        line.pop_back();
        lines.push_back(line);
    }
    CfgData cfg;
    ASSERT_TRUE(detail::parseCfgLines(lines, cfg));
    ReadOptions options;
    Warnings logged;
    options.warning_logger = [&](const std::string& source, const Warning& warning) {
        EXPECT_EQ(source, "DAT stream");
        logged.push_back(warning);
    };
    StreamReader reader(cfg, options);
    std::istringstream dat("1,0,0,0\n2,1000,0,1\n");
    EXPECT_EQ(reader.processStream(dat, [](const auto&) {}), 2u);
    const auto* warning = findWarning(logged, "ieee.dat.crlf");
    ASSERT_NE(warning, nullptr);
    EXPECT_EQ(warning->count, 2u);
    logged.clear();
    std::istringstream good("1,0,0,0\r\n2,1000,0,1\r\n\x1a");
    EXPECT_EQ(reader.processStream(good, [](const auto&) {}), 2u);
    EXPECT_TRUE(logged.empty());
    EXPECT_EQ(reader.getDatTail().byte_count, 0u);
}

TEST(FormatValidation, RecordLogsCfgWarningsAndFailedOpen) {
    const std::filesystem::path path("format-validation-logging.cfg");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{path};
    auto data = configuration();
    data.erase(data.find('\r'), 1);
    { std::ofstream file(path, std::ios::binary); file << data; }
    ReadOptions options;
    Warnings logged;
    options.warning_logger = [&](const auto&, const auto& warning) { logged.push_back(warning); };
    Record record;
    record.setReadOptions(options);
    ASSERT_TRUE(record.parseCfg(path.string()));
    EXPECT_NE(findWarning(logged, "ieee.cfg.crlf"), nullptr);
    logged.clear();
    EXPECT_FALSE(record.parseDat(path.string()+".missing"));
    EXPECT_NE(findWarning(logged, "dat.open"), nullptr);
}

} // namespace

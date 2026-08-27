#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "comtrade/stream_reader.hpp"

namespace {

namespace fs = std::filesystem;

constexpr auto kFixtureStem = "SIMENS/20191024045947";

fs::path fixturePath(const char* extension) {
    return fs::path(COMTRADE_TEST_DATA_DIR) / (std::string(kFixtureStem) + extension);
}

bool isGitLfsPointer(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::string first_line;
    std::getline(input, first_line);
    return first_line.rfind("version https://git-lfs.github.com/spec/v1", 0) == 0;
}

TEST(ComtradeRealFileAccuracy, SiemensAsciiRecordingMatchesReferenceValues) {
    const auto cfg_path = fixturePath(".CFG");
    const auto dat_path = fixturePath(".DAT");
    ASSERT_TRUE(fs::exists(cfg_path)) << cfg_path;
    ASSERT_TRUE(fs::exists(dat_path)) << dat_path;
    if (isGitLfsPointer(cfg_path) || isGitLfsPointer(dat_path)) {
        GTEST_SKIP() << "Real COMTRADE fixture is a Git LFS pointer; run git lfs pull first.";
    }

    const comtrade::StreamReader reader(cfg_path.string());
    const auto& cfg = reader.getCfg();
    EXPECT_EQ(cfg.total_channels, 41);
    EXPECT_EQ(cfg.analog_count, 12);
    EXPECT_EQ(cfg.digital_count, 29);
    EXPECT_EQ(cfg.data_type, comtrade::DataType::ASCII);
    EXPECT_DOUBLE_EQ(cfg.line_frequency, 50.0);
    ASSERT_EQ(cfg.sample_rates.size(), 1U);
    EXPECT_DOUBLE_EQ(cfg.sample_rates[0].samples_per_second, 800.0);
    EXPECT_EQ(cfg.sample_rates[0].end_sample, 1740U);

    std::array<double, 12> analog_sums{};
    std::array<std::uint32_t, 29> digital_true_counts{};
    comtrade::SampleRow first_row;
    comtrade::SampleRow last_row;
    std::size_t row_count = 0;
    std::uint32_t previous_timestamp = 0;
    std::size_t intervals_1249 = 0;
    std::size_t intervals_1251 = 0;
    std::size_t timestamps_one_microsecond_early = 0;

    const auto processed = reader.processDatStream(dat_path.string(), [&](const comtrade::SampleRow& row) {
        ASSERT_EQ(row.analog_values.size(), analog_sums.size());
        ASSERT_EQ(row.digital_values.size(), digital_true_counts.size());
        EXPECT_EQ(row.index, row_count + 1);
        EXPECT_EQ(row.timestamp_us, row.raw_timestamp);
        const auto ideal_timestamp = static_cast<std::uint32_t>(row_count * 1250U);
        ASSERT_TRUE(row.raw_timestamp == ideal_timestamp || row.raw_timestamp + 1U == ideal_timestamp);
        if (row.raw_timestamp + 1U == ideal_timestamp) ++timestamps_one_microsecond_early;
        if (row_count > 0) {
            const auto interval = row.raw_timestamp - previous_timestamp;
            ASSERT_GE(interval, 1249U);
            ASSERT_LE(interval, 1251U);
            if (interval == 1249U) ++intervals_1249;
            if (interval == 1251U) ++intervals_1251;
        }
        previous_timestamp = row.raw_timestamp;

        for (std::size_t i = 0; i < analog_sums.size(); ++i) {
            analog_sums[i] += row.analog_values[i];
        }
        for (std::size_t i = 0; i < digital_true_counts.size(); ++i) {
            digital_true_counts[i] += row.digital_values[i] ? 1U : 0U;
        }

        if (row_count == 0) first_row = row;
        last_row = row;
        ++row_count;
    });

    EXPECT_EQ(processed, 1740U);
    EXPECT_EQ(row_count, 1740U);
    EXPECT_EQ(timestamps_one_microsecond_early, 24U);
    EXPECT_EQ(intervals_1249, 23U);
    EXPECT_EQ(intervals_1251, 23U);

    constexpr std::array<double, 12> expected_sums = {
        0.06961520664005338, -0.08432408385002621, 2.5546920344200474,
        -2.5391913844602243, -114.75848753280292, -14.324892020402622,
        211.8075837278036, 47.80226800160778, 0.00040013714284740774,
        -100.41497856000007, -226.1375227229966, 326.6305204220019
    };
    constexpr std::array<std::uint32_t, 29> expected_digital_counts = {
        0, 0, 0, 1298, 1298, 1291, 0, 1300, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1448, 1369, 1740, 1368, 1296, 576
    };

    for (std::size_t i = 0; i < analog_sums.size(); ++i) {
        EXPECT_NEAR(analog_sums[i], expected_sums[i], 1e-9) << "analog channel " << i;
    }
    EXPECT_EQ(digital_true_counts, expected_digital_counts);

    EXPECT_EQ(first_row.index, 1U);
    EXPECT_EQ(first_row.raw_timestamp, 0U);
    EXPECT_NEAR(first_row.analog_values[0], 0.30226265148, 1e-12);
    EXPECT_NEAR(first_row.analog_values[4], 5.3455147212, 1e-12);
    EXPECT_TRUE(first_row.digital_values[25]);

    EXPECT_EQ(last_row.index, 1740U);
    EXPECT_EQ(last_row.raw_timestamp, 2173750U);
    EXPECT_NEAR(last_row.analog_values[0], 0.52142214432, 1e-12);
    EXPECT_NEAR(last_row.analog_values[11], -117.137702218, 1e-9);
    EXPECT_TRUE(last_row.digital_values[25]);
}

}  // namespace

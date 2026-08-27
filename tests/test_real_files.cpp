#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "comtrade/stream_reader.hpp"

// 真实文件测试负责设备兼容性；固定的小型合成边界用例放在 test_stream.cpp。
namespace {

namespace fs = std::filesystem;

constexpr auto kFixtureStem = "SIMENS/20191024045947";

struct AsciiFixture {
    const char* test_name;
    const char* cfg_relative_path;
    const char* dat_relative_path;
    std::size_t expected_rows;
};

// 清单只包含 CFG 声明为 ASCII 且具有同名 DAT 的记录，名称同时标识源文本编码。
constexpr std::array<AsciiFixture, 6> kAsciiFixtures = {{
    {"Gb2312Cometrade000002", "cometrade/000002.CFG", "cometrade/000002.DAT", 4381},
    {"Gb2312Cometrade000003", "cometrade/000003.CFG", "cometrade/000003.DAT", 4381},
    {"Gb2312Cometrade000004", "cometrade/000004.CFG", "cometrade/000004.DAT", 4381},
    {"Gb2312Siemens20191024045947", "SIMENS/20191024045947.CFG", "SIMENS/20191024045947.DAT", 1740},
    {"Utf8TransientRecording1",
     "Transient/example_station_name_Recording_Device_ID_1_20260822_055921_697_TRIP.cfg",
     "Transient/example_station_name_Recording_Device_ID_1_20260822_055921_697_TRIP.dat", 256001},
    {"Utf8TransientRecording2",
     "Transient/example_station_name_Recording_Device_ID_2_20260822_055933_216_TRIP.cfg",
     "Transient/example_station_name_Recording_Device_ID_2_20260822_055933_216_TRIP.dat", 28927},
}};

fs::path dataPath(const char* relative_path) {
    return fs::path(COMTRADE_TEST_DATA_DIR) / relative_path;
}

fs::path fixturePath(const char* extension) {
    return fs::path(COMTRADE_TEST_DATA_DIR) / (std::string(kFixtureStem) + extension);
}

bool isGitLfsPointer(const fs::path& path) {

    std::ifstream input(path, std::ios::binary);

    // 未执行 git lfs pull 时仓库里只有指针文本，此时跳过而不是报告解析器错误。
    std::string first_line;
    std::getline(input, first_line);
    return first_line.rfind("version https://git-lfs.github.com/spec/v1", 0) == 0;
}

class ComtradeAsciiFileTest : public ::testing::TestWithParam<AsciiFixture> {};

TEST_P(ComtradeAsciiFileTest, StreamsEverySampleWithConsistentTimeAndChannelShape) {
    const auto& fixture = GetParam();
    const auto cfg_path = dataPath(fixture.cfg_relative_path);
    const auto dat_path = dataPath(fixture.dat_relative_path);
    SCOPED_TRACE(fixture.test_name);

    ASSERT_TRUE(fs::exists(cfg_path)) << cfg_path;
    ASSERT_TRUE(fs::exists(dat_path)) << dat_path;
    if (isGitLfsPointer(cfg_path) || isGitLfsPointer(dat_path)) {
        GTEST_SKIP() << "COMTRADE fixture is a Git LFS pointer; run git lfs pull first: " << cfg_path;
    }

    // 通用层验证每份录波的结构、编码、时间换算和完整行数，不绑定具体波形值。
    const comtrade::StreamReader reader(cfg_path.string());
    const auto& cfg = reader.getCfg();
    ASSERT_EQ(cfg.data_type, comtrade::DataType::ASCII);
    EXPECT_TRUE(comtrade::detail::isValidUtf8(cfg.station_name));
    EXPECT_TRUE(comtrade::detail::isValidUtf8(cfg.rec_dev_id));
    ASSERT_EQ(cfg.analog_channels.size(), static_cast<std::size_t>(cfg.analog_count));
    ASSERT_EQ(cfg.digital_channels.size(), static_cast<std::size_t>(cfg.digital_count));
    for (const auto& channel : cfg.analog_channels) {
        EXPECT_TRUE(comtrade::detail::isValidUtf8(channel.id));
        EXPECT_TRUE(comtrade::detail::isValidUtf8(channel.phase));
        EXPECT_TRUE(comtrade::detail::isValidUtf8(channel.ccbm));
        EXPECT_TRUE(comtrade::detail::isValidUtf8(channel.uu));
    }
    for (const auto& channel : cfg.digital_channels) {
        EXPECT_TRUE(comtrade::detail::isValidUtf8(channel.id));
        EXPECT_TRUE(comtrade::detail::isValidUtf8(channel.phase));
        EXPECT_TRUE(comtrade::detail::isValidUtf8(channel.ccbm));
    }
    ASSERT_FALSE(cfg.sample_rates.empty());
    EXPECT_EQ(cfg.sample_rates.back().end_sample, fixture.expected_rows);

    std::size_t callback_count = 0;
    std::uint32_t previous_raw_timestamp = 0;
    const auto processed = reader.processDatStream(dat_path.string(), [&](const comtrade::SampleRow& row) {
        EXPECT_EQ(row.index, callback_count + 1);
        EXPECT_EQ(row.analog_values.size(), static_cast<std::size_t>(cfg.analog_count));
        EXPECT_EQ(row.digital_values.size(), static_cast<std::size_t>(cfg.digital_count));

        if (callback_count > 0) EXPECT_GE(row.raw_timestamp, previous_raw_timestamp);
        previous_raw_timestamp = row.raw_timestamp;

        // 独立重算时间公式，确保 raw、TIMEMULT、纳秒偏移和绝对时间相互一致。
        const auto expected_offset_ns = static_cast<std::int64_t>(std::llround(
            static_cast<long double>(row.raw_timestamp) *
            static_cast<long double>(cfg.time_multiplier) * 1000.0L));
        EXPECT_EQ(row.time_offset.count(), expected_offset_ns);
        EXPECT_EQ(row.timestamp_us,
                  static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::nanoseconds(expected_offset_ns)).count()));
        EXPECT_EQ(row.absolute_time, cfg.start_time + std::chrono::nanoseconds(expected_offset_ns));
        ++callback_count;
    });

    EXPECT_EQ(processed, fixture.expected_rows);
    EXPECT_EQ(callback_count, fixture.expected_rows);
}

INSTANTIATE_TEST_SUITE_P(
    AllAsciiRecordings,
    ComtradeAsciiFileTest,
    ::testing::ValuesIn(kAsciiFixtures),
    [](const ::testing::TestParamInfo<AsciiFixture>& info) { return info.param.test_name; });

TEST(ComtradeRealFileAccuracy, SiemensAsciiRecordingMatchesReferenceValues) {
    // Siemens 文件额外使用预先计算的黄金基线，检查数值正确性而不只是“能够读取”。
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

    // 累计值和置位次数由库外独立计算，避免用被测实现生成自身期望结果。
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

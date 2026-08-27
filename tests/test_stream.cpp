#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include "comtrade/record.hpp"
#include "comtrade/stream_reader.hpp"
#include "comtrade/stream_writer.hpp"
#include "comtrade/types.hpp"

// 流式引擎测试同时覆盖 ASCII 文本、三种二进制布局、时间精度和 CFG 编码兼容。
namespace {
    namespace fs = std::filesystem;

    // 构造约束属于公开 API 契约：Reader 必须绑定有效 CFG，且不允许隐式字符串转换。
    static_assert(!std::is_default_constructible_v<comtrade::StreamReader>);
    static_assert(std::is_constructible_v<comtrade::StreamReader, const std::string &>);
    static_assert(!std::is_convertible_v<std::string, comtrade::StreamReader>);

    class StreamEngineTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // 用测试套件/用例名生成隔离目录，支持 CTest 并行执行而不互相覆盖文件。
            const auto *test_info = ::testing::UnitTest::GetInstance()->current_test_info();
            test_directory_ = fs::temp_directory_path() /
                              (std::string("comtrade_") + test_info->test_suite_name() + "_" + test_info->name());

            std::error_code error;
            fs::remove_all(test_directory_, error);
            ASSERT_TRUE(fs::create_directories(test_directory_));
            cfg_path_ = test_directory_ / "samples.cfg";
            dat_path_ = test_directory_ / "samples.dat";
        }

        void TearDown() override {
            std::error_code error;
            fs::remove_all(test_directory_, error);
        }

        static comtrade::CfgData makeCfg(comtrade::DataType data_type,
                                         std::size_t analog_count,
                                         std::size_t digital_count) {
            // 为各 DAT 编码测试构造最小但内部计数一致的 CFG。
            comtrade::CfgData cfg;
            cfg.data_type = data_type;

            for (std::size_t i = 0; i < analog_count; ++i) {
                comtrade::AnalogChannel channel;
                channel.index = static_cast<int>(i + 1);
                channel.id = "A" + std::to_string(i + 1);
                cfg.analog_channels.push_back(channel);
            }

            for (std::size_t i = 0; i < digital_count; ++i) {
                comtrade::DigitalChannel channel;
                channel.index = static_cast<int>(i + 1);
                channel.id = "D" + std::to_string(i + 1);
                cfg.digital_channels.push_back(channel);
            }

            cfg.analog_count = static_cast<int>(analog_count);
            cfg.digital_count = static_cast<int>(digital_count);
            cfg.total_channels = cfg.analog_count + cfg.digital_count;
            return cfg;
        }

        static std::string readTextFile(const fs::path &path) {
            std::ifstream input(path, std::ios::binary);
            return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        }

        static std::vector<char> readBinaryFile(const fs::path &path) {
            std::ifstream input(path, std::ios::binary);
            return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        }

        template<typename T>
        static T readValue(const std::vector<char> &bytes, std::size_t offset) {
            // memcpy 读取避免测试代码本身因未对齐 reinterpret_cast 产生未定义行为。
            T value{};
            EXPECT_LE(offset + sizeof(T), bytes.size());
            if (offset + sizeof(T) <= bytes.size()) {
                std::memcpy(&value, bytes.data() + offset, sizeof(T));
            }
            return value;
        }

        fs::path test_directory_;
        fs::path cfg_path_;
        fs::path dat_path_;
    };

    TEST_F(StreamEngineTest, PushRowRequiresAnOpenStream) {
        comtrade::StreamWriter writer(makeCfg(comtrade::DataType::ASCII, 0, 0));

        EXPECT_THROW(writer.pushRow(0, {}, {}), std::runtime_error);
    }

    TEST_F(StreamEngineTest, WritesAsciiRowsWithSequentialIndexesAndScaledAnalogValues) {
        auto cfg = makeCfg(comtrade::DataType::ASCII, 2, 3);
        cfg.analog_channels[0].a = 0.5;
        cfg.analog_channels[0].b = -1.0;
        cfg.analog_channels[1].a = 2.0;
        cfg.analog_channels[1].b = 5.0;

        comtrade::StreamWriter writer(cfg);
        ASSERT_TRUE(writer.open(dat_path_.string()));

        writer.pushRow(100, {10.0, -3.0}, {true, false, true});
        writer.pushRow(250, {-1.0, 9.0}, {false, true, false});
        writer.close();

        EXPECT_EQ(readTextFile(dat_path_),
                  "1,100,22,-4,1,0,1\r\n"
                  "2,250,0,2,0,1,0\r\n");
    }

    TEST_F(StreamEngineTest, WritesBinaryRowsUsingInt16AnalogsAndPackedDigitalWords) {
        auto cfg = makeCfg(comtrade::DataType::BINARY, 2, 17);
        cfg.analog_channels[0].a = 0.25;
        cfg.analog_channels[0].b = 1.0;
        cfg.analog_channels[1].a = 2.0;
        cfg.analog_channels[1].b = -5.0;

        // 17 路数字量跨越两个 16-bit word，可验证边界位和第二个 word 的布局。
        std::vector<bool> digitals(17, false);
        digitals[0] = true;
        digitals[3] = true;
        digitals[15] = true;
        digitals[16] = true;

        comtrade::StreamWriter writer(cfg);
        ASSERT_TRUE(writer.open(dat_path_.string()));
        writer.pushRow(123456, {3.5, -1.0}, digitals);
        writer.close();

        const auto bytes = readBinaryFile(dat_path_);
        ASSERT_EQ(bytes.size(), 16U); // index + timestamp + 2 analogs + 2 digital words
        EXPECT_EQ(readValue<std::uint32_t>(bytes, 0), 1U);
        EXPECT_EQ(readValue<std::uint32_t>(bytes, 4), 123456U);
        EXPECT_EQ(readValue<std::int16_t>(bytes, 8), 10);
        EXPECT_EQ(readValue<std::int16_t>(bytes, 10), 2);
        EXPECT_EQ(readValue<std::uint16_t>(bytes, 12), 0x8009U);
        EXPECT_EQ(readValue<std::uint16_t>(bytes, 14), 0x0001U);
    }

    TEST_F(StreamEngineTest, WritesBinary32RowsUsingInt32Analogs) {
        auto cfg = makeCfg(comtrade::DataType::BINARY32, 1, 0);
        cfg.analog_channels[0].a = 0.001;
        cfg.analog_channels[0].b = -10.0;

        comtrade::StreamWriter writer(cfg);
        ASSERT_TRUE(writer.open(dat_path_.string()));
        writer.pushRow(42, {123446.789}, {});
        writer.pushRow(84, {-10.0}, {});
        writer.close();

        const auto bytes = readBinaryFile(dat_path_);
        ASSERT_EQ(bytes.size(), 24U);
        EXPECT_EQ(readValue<std::uint32_t>(bytes, 0), 1U);
        EXPECT_EQ(readValue<std::uint32_t>(bytes, 4), 42U);
        EXPECT_EQ(readValue<std::int32_t>(bytes, 8), 123456789);
        EXPECT_EQ(readValue<std::uint32_t>(bytes, 12), 2U);
        EXPECT_EQ(readValue<std::uint32_t>(bytes, 16), 84U);
        EXPECT_EQ(readValue<std::int32_t>(bytes, 20), 0);
    }

    TEST_F(StreamEngineTest, WritesFloat32RowsAndAppliesTheTimeMultiplier) {
        auto cfg = makeCfg(comtrade::DataType::FLOAT32, 1, 0);
        cfg.analog_channels[0].a = 0.5;
        cfg.analog_channels[0].b = 1.0;
        cfg.time_multiplier = 0.001;

        comtrade::StreamWriter writer(cfg);
        ASSERT_TRUE(writer.open(dat_path_.string()));
        writer.pushRow(250, {3.5}, {});
        writer.close();

        const auto bytes = readBinaryFile(dat_path_);
        ASSERT_EQ(bytes.size(), 12U);
        EXPECT_EQ(readValue<std::uint32_t>(bytes, 0), 1U);
        EXPECT_EQ(readValue<std::uint32_t>(bytes, 4), 250000U);
        EXPECT_FLOAT_EQ(readValue<float>(bytes, 8), 5.0F);
    }

    TEST_F(StreamEngineTest, ReaderReturnsZeroWhenTheDatFileCannotBeOpened) {
        comtrade::Record generated_record;
        generated_record.setStationAndDevice("GRID_01", "RELAY_01");
        ASSERT_TRUE(generated_record.saveCfg(cfg_path_.string()));

        comtrade::StreamReader reader(cfg_path_.string());
        std::size_t callback_count = 0;

        const auto row_count = reader.processDatStream(
            (test_directory_ / "missing.dat").string(),
            [&](const comtrade::SampleRow &) { ++callback_count; });

        EXPECT_EQ(row_count, 0U);
        EXPECT_EQ(callback_count, 0U);
    }

    TEST_F(StreamEngineTest, ReaderConstructorRejectsMissingOrMalformedCfg) {
        EXPECT_THROW(
            static_cast<void>(comtrade::StreamReader((test_directory_ / "missing.cfg").string())),
            std::runtime_error); {
            std::ofstream malformed_cfg(cfg_path_);
            ASSERT_TRUE(malformed_cfg.is_open());
            malformed_cfg << "this is not a COMTRADE configuration\n";
        }

        EXPECT_THROW(static_cast<void>(comtrade::StreamReader(cfg_path_.string())), std::runtime_error);
    }

    TEST_F(StreamEngineTest, ReaderAcceptsGb2312VendorCfgWithZeroSampleRate) {
        // 用字节转义构造编码样本，避免测试源码自身编码影响 Windows/MSVC 编译结果。
        const std::string gb2312_station = "\xB2\xE2\xCA\xD4"; // GB2312: 测试
        const std::string utf8_station = "\xE6\xB5\x8B\xE8\xAF\x95";
        {
            std::ofstream cfg_file(cfg_path_, std::ios::binary);
            ASSERT_TRUE(cfg_file.is_open());
            cfg_file << gb2312_station << ",DEVICE,1997\r\n"
                     << "0,0A,0D\r\n"
                     << "50\r\n"
                     << "0\r\n"
                     << "0,2\r\n"
                     << "01/07/2020,11:54:47.705000\r\n"
                     << "01/07/2020,11:54:47.955000\r\n"
                     << "ASCII\r\n"
                     << "0.001\r\n";
        }

        const comtrade::StreamReader reader(cfg_path_.string());
        const auto& cfg = reader.getCfg();
        EXPECT_EQ(cfg.station_name, utf8_station);
        EXPECT_EQ(cfg.data_type, comtrade::DataType::ASCII);
        ASSERT_EQ(cfg.sample_rates.size(), 1U);
        EXPECT_DOUBLE_EQ(cfg.sample_rates[0].samples_per_second, 0.0);
        EXPECT_EQ(cfg.sample_rates[0].end_sample, 2U);
        EXPECT_DOUBLE_EQ(cfg.time_multiplier, 0.001);
    }

    TEST_F(StreamEngineTest, ReaderPreservesUtf8MetadataAndRemovesBom) {
        const std::string utf8_station = "\xE6\xB5\x8B\xE8\xAF\x95";
        {
            std::ofstream cfg_file(cfg_path_, std::ios::binary);
            ASSERT_TRUE(cfg_file.is_open());
            cfg_file << "\xEF\xBB\xBF" << utf8_station << ",DEVICE,1999\r\n"
                     << "0,0A,0D\r\n"
                     << "50\r\n"
                     << "1\r\n"
                     << "1000,1\r\n"
                     << "01/07/2020,11:54:47.705000\r\n"
                     << "01/07/2020,11:54:47.955000\r\n"
                     << "ASCII\r\n"
                     << "1.0\r\n";
        }

        const comtrade::StreamReader reader(cfg_path_.string());
        EXPECT_EQ(reader.getCfg().station_name, utf8_station);
    }

    TEST_F(StreamEngineTest, GeneratesComtradeFilesAndStreamsEverySample) {
        // 端到端验证 Writer/Record 生成的 CRLF 文件可被 Reader 逐行还原。
        comtrade::Record generated_record;
        generated_record.setStationAndDevice("GRID_01", "RELAY_01", comtrade::StandardVersion::V1999);

        comtrade::AnalogChannel voltage;
        voltage.index = 1;
        voltage.id = "VA";
        voltage.phase = "A";
        voltage.uu = "V";
        voltage.a = 0.1;
        voltage.b = -5.0;
        generated_record.addAnalogChannel(voltage);

        comtrade::AnalogChannel current;
        current.index = 2;
        current.id = "IA";
        current.phase = "A";
        current.uu = "A";
        current.a = 0.01;
        generated_record.addAnalogChannel(current);

        comtrade::DigitalChannel trip;
        trip.index = 1;
        trip.id = "TRIP";
        generated_record.addDigitalChannel(trip);

        comtrade::DigitalChannel breaker_closed;
        breaker_closed.index = 2;
        breaker_closed.id = "BREAKER_CLOSED";
        breaker_closed.normal_state = 1;
        generated_record.addDigitalChannel(breaker_closed);

        generated_record.addSample(0, {220.0, 5.25}, {false, true});
        generated_record.addSample(250, {221.2, -1.5}, {true, false});
        generated_record.addSample(500, {-5.0, 0.0}, {true, true});
        generated_record.getMutableCfg().data_type = comtrade::DataType::ASCII;

        ASSERT_TRUE(generated_record.saveCfg(cfg_path_.string()));
        ASSERT_TRUE(generated_record.saveDat(dat_path_.string()));
        ASSERT_TRUE(fs::exists(cfg_path_));
        ASSERT_TRUE(fs::exists(dat_path_));

        comtrade::StreamReader reader(cfg_path_.string());
        EXPECT_EQ(reader.getCfg().station_name, "GRID_01");
        EXPECT_EQ(reader.getCfg().rec_dev_id, "RELAY_01");
        EXPECT_EQ(reader.getCfg().analog_count, 2);
        EXPECT_EQ(reader.getCfg().digital_count, 2);

        std::vector<comtrade::SampleRow> rows;
        // 地址保持不变证明 Reader 复用单行缓冲；rows.push_back 则示范需要长期保存时应复制。
        const comtrade::SampleRow *reused_row = nullptr;
        const auto row_count = reader.processDatStream(dat_path_.string(), [&](const comtrade::SampleRow &row) {
            if (reused_row == nullptr) {
                reused_row = &row;
            } else {
                EXPECT_EQ(&row, reused_row);
            }
            rows.push_back(row);
        });

        EXPECT_EQ(row_count, 3U);
        ASSERT_EQ(rows.size(), 3U);

        EXPECT_EQ(rows[0].index, 1U);
        EXPECT_EQ(rows[0].timestamp_us, 0U);
        EXPECT_NEAR(rows[0].analog_values[0], 220.0, 1e-9);
        EXPECT_NEAR(rows[0].analog_values[1], 5.25, 1e-9);
        EXPECT_FALSE(rows[0].digital_values[0]);
        EXPECT_TRUE(rows[0].digital_values[1]);

        EXPECT_EQ(rows[1].index, 2U);
        EXPECT_EQ(rows[1].timestamp_us, 250U);
        EXPECT_NEAR(rows[1].analog_values[0], 221.2, 1e-9);
        EXPECT_NEAR(rows[1].analog_values[1], -1.5, 1e-9);
        EXPECT_TRUE(rows[1].digital_values[0]);
        EXPECT_FALSE(rows[1].digital_values[1]);

        EXPECT_EQ(rows[2].index, 3U);
        EXPECT_EQ(rows[2].timestamp_us, 500U);
        EXPECT_NEAR(rows[2].analog_values[0], -5.0, 1e-9);
        EXPECT_NEAR(rows[2].analog_values[1], 0.0, 1e-9);
        EXPECT_TRUE(rows[2].digital_values[0]);
        EXPECT_TRUE(rows[2].digital_values[1]);
    }

    TEST_F(StreamEngineTest, Comtrade2013CfgPreservesPreciseTimeMetadata) {
        // 覆盖 2013 专有尾部字段、9 位小数秒以及小于 1 微秒的 TIMEMULT。
        comtrade::Record generated_record;
        generated_record.setStationAndDevice("GRID_2013", "RELAY_2013", comtrade::StandardVersion::V2013);

        comtrade::AnalogChannel voltage;
        voltage.index = 1;
        voltage.id = "VA";
        voltage.uu = "V";
        voltage.a = 0.1;
        generated_record.addAnalogChannel(voltage);
        generated_record.addSample(0, {220.0}, {});
        generated_record.addSample(250, {221.0}, {});
        generated_record.setTimestamps(
            "19/08/2026,12:34:56.123456789",
            "19/08/2026,12:34:56.123706789");

        auto &cfg = generated_record.getMutableCfg();
        cfg.data_type = comtrade::DataType::ASCII;
        cfg.sample_rates = {{4000.0, 2}};
        cfg.time_multiplier = 0.001;
        cfg.time_code = "UTC";
        cfg.local_code = "+0800";
        cfg.time_quality_code = "4";
        cfg.leap_second = 1;
        cfg.timestamp_fractional_digits = 9;

        ASSERT_TRUE(generated_record.saveCfg(cfg_path_.string()));
        ASSERT_TRUE(generated_record.saveDat(dat_path_.string()));

        const auto cfg_text = readTextFile(cfg_path_);
        EXPECT_NE(cfg_text.find("GRID_2013,RELAY_2013,2013\r\n"), std::string::npos);
        EXPECT_NE(cfg_text.find("19/08/2026,12:34:56.123456789\r\n"), std::string::npos);
        EXPECT_NE(cfg_text.find("4000,2\r\n"), std::string::npos);
        EXPECT_NE(cfg_text.find("ASCII\r\n0.001\r\nUTC,+0800\r\n4,1\r\n"), std::string::npos);

        comtrade::StreamReader reader(cfg_path_.string());
        const auto &parsed_cfg = reader.getCfg();
        EXPECT_EQ(parsed_cfg.version, comtrade::StandardVersion::V2013);
        ASSERT_EQ(parsed_cfg.sample_rates.size(), 1U);
        EXPECT_DOUBLE_EQ(parsed_cfg.sample_rates[0].samples_per_second, 4000.0);
        EXPECT_EQ(parsed_cfg.sample_rates[0].end_sample, 2U);
        EXPECT_DOUBLE_EQ(parsed_cfg.time_multiplier, 0.001);
        EXPECT_EQ(parsed_cfg.time_code, "UTC");
        EXPECT_EQ(parsed_cfg.local_code, "+0800");
        EXPECT_EQ(parsed_cfg.time_quality_code, "4");
        EXPECT_EQ(parsed_cfg.leap_second, 1);
        EXPECT_EQ(parsed_cfg.timestamp_fractional_digits, 9);

        std::vector<comtrade::SampleRow> rows;
        ASSERT_EQ(reader.processDatStream(dat_path_.string(), [&](const auto& row) { rows.push_back(row); }), 2U);
        ASSERT_EQ(rows.size(), 2U);
        EXPECT_EQ(rows[1].raw_timestamp, 250000U);
        EXPECT_EQ(rows[1].timestamp_us, 250U);
        EXPECT_EQ(rows[1].time_offset, std::chrono::microseconds(250));
        EXPECT_EQ(rows[1].absolute_time, parsed_cfg.start_time + std::chrono::microseconds(250));
    }
} // namespace

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
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

namespace {

namespace fs = std::filesystem;

static_assert(!std::is_default_constructible_v<comtrade::StreamReader>);
static_assert(std::is_constructible_v<comtrade::StreamReader, const std::string&>);
static_assert(!std::is_convertible_v<std::string, comtrade::StreamReader>);

class StreamEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
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

    static std::string readTextFile(const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    static std::vector<char> readBinaryFile(const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    template <typename T>
    static T readValue(const std::vector<char>& bytes, std::size_t offset) {
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
              "1,100,22,-4,1,0,1\n"
              "2,250,0,2,0,1,0\n");
}

TEST_F(StreamEngineTest, WritesBinaryRowsUsingInt16AnalogsAndPackedDigitalWords) {
    auto cfg = makeCfg(comtrade::DataType::BINARY, 2, 17);
    cfg.analog_channels[0].a = 0.25;
    cfg.analog_channels[0].b = 1.0;
    cfg.analog_channels[1].a = 2.0;
    cfg.analog_channels[1].b = -5.0;

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
    ASSERT_EQ(bytes.size(), 16U);  // index + timestamp + 2 analogs + 2 digital words
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

TEST_F(StreamEngineTest, ReaderReturnsZeroWhenTheDatFileCannotBeOpened) {
    comtrade::Record generated_record;
    generated_record.setStationAndDevice("GRID_01", "RELAY_01");
    ASSERT_TRUE(generated_record.saveCfg(cfg_path_.string()));

    comtrade::StreamReader reader(cfg_path_.string());
    std::size_t callback_count = 0;

    const auto row_count = reader.processDatStream(
        (test_directory_ / "missing.dat").string(),
        [&](const comtrade::SampleRow&) { ++callback_count; });

    EXPECT_EQ(row_count, 0U);
    EXPECT_EQ(callback_count, 0U);
}

TEST_F(StreamEngineTest, ReaderConstructorRejectsMissingOrMalformedCfg) {
    EXPECT_THROW(
        static_cast<void>(comtrade::StreamReader((test_directory_ / "missing.cfg").string())),
        std::runtime_error);

    {
        std::ofstream malformed_cfg(cfg_path_);
        ASSERT_TRUE(malformed_cfg.is_open());
        malformed_cfg << "this is not a COMTRADE configuration\n";
    }

    EXPECT_THROW(static_cast<void>(comtrade::StreamReader(cfg_path_.string())), std::runtime_error);
}

TEST_F(StreamEngineTest, GeneratesComtradeFilesAndStreamsEverySample) {
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
    const comtrade::SampleRow* reused_row = nullptr;
    const auto row_count = reader.processDatStream(dat_path_.string(), [&](const comtrade::SampleRow& row) {
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

}  // namespace

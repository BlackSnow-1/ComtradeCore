#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <vector>
#include <cstdint>

// 包含核心头文件
#include "comtrade/types.hpp"
#include "comtrade/stream_writer.hpp"
#include "comtrade/stream_reader.hpp"

namespace fs = std::filesystem;
using namespace std::chrono;

// 定义 Test Fixture，管理临时文件和通用逻辑
class ComtradeStreamTest : public ::testing::Test {
protected:
    const std::string cfg_path = "test_stream.cfg";
    const std::string dat_path = "test_stream.dat";

    void SetUp() override {
        cleanUpFiles();
    }

    void TearDown() override {
        cleanUpFiles();
    }

    void cleanUpFiles() {
        if (fs::exists(cfg_path)) fs::remove(cfg_path);
        if (fs::exists(dat_path)) fs::remove(dat_path);
    }

    // 构造模拟的 CfgData 元数据
    static comtrade::CfgData createDummyCfg(const std::string& format) {
        comtrade::CfgData cfg;
        cfg.station_name = "Substation_A";
        cfg.rec_dev_id = "Relay_1";

        // 使用逐个字段赋值的方式，彻底避免不同编译器对 {} 聚合初始化的严苛限制报错
        comtrade::AnalogChannel ach;
        ach.index = 1;
        ach.id = "Va";
        ach.phase = "PhaseA";
        ach.ccbm = "";
        ach.uu = "V";
        ach.a = 1.0;
        ach.b = 0.0;
        ach.skew = 0.0;
        ach.min = -1000;
        ach.max = 1000;
        ach.primary = 1.0;
        ach.secondary = 1.0;
        ach.ps = "S";
        cfg.analog_channels.push_back(ach);

        comtrade::DigitalChannel dch;
        dch.index = 1;
        dch.id = "CB1_Status";
        dch.phase = "Breaker";
        dch.ccbm = "";
        dch.normal_state = 0;
        cfg.digital_channels.push_back(dch);

        cfg.analog_count = 1;
        cfg.digital_count = 1;
        cfg.total_channels = 2;

        // 调用我们刚刚新增的工具类转换格式
        cfg.data_type = comtrade::DataTypeUtils::FromString(format);
        return cfg;
    }
};

// =========================================================================
// 测试 1：ASCII 格式的流式连续多行读写完整性
// =========================================================================
TEST_F(ComtradeStreamTest, AsciiContinuousStreamRoundTrip) {
    auto cfg = createDummyCfg("ASCII");
    auto base_time = system_clock::now();
    const int total_rows = 100; // 模拟连续写入和读取 100 行流数据

    // 1. 测试流式写入
    {
        comtrade::StreamWriter writer(cfg_path, dat_path, cfg);

        for (int i = 0; i < total_rows; ++i) {
            // 模拟连续的数据变化
            double analog_val = 220.5 + i * 0.1;
            uint32_t digital_val = i % 2; // 0和1交替

            auto ts = base_time + microseconds(i * 100); // 模拟时间戳递增
            bool write_ok = writer.writeRow(ts, {analog_val}, {digital_val});
            ASSERT_TRUE(write_ok) << "Failed to write row " << i;
        }
    } // 离开作用域，writer 析构，确保文件流关闭且 cfg 尾部信息写入完成

    // 2. 测试流式读取
    {
        comtrade::StreamReader reader(cfg_path, dat_path);

        // 验证元数据解析
        auto read_cfg = reader.getCfg();
        EXPECT_EQ(read_cfg.station_name, "Substation_A");
        EXPECT_EQ(read_cfg.data_type, comtrade::DataType::ASCII); // 已修正：检查 data_type

        comtrade::TimePoint read_ts;
        std::vector<double> read_analogs;
        std::vector<uint32_t> read_digitals;

        int rows_read = 0;
        // 验证游标式的连续读取
        while (reader.readNext(read_ts, read_analogs, read_digitals)) {
            EXPECT_EQ(read_analogs.size(), 1);
            EXPECT_EQ(read_digitals.size(), 1);

            double expected_analog = 220.5 + rows_read * 0.1;
            uint32_t expected_digital = rows_read % 2;

            // 重要：ASCII格式存在字符串来回转换，使用 EXPECT_NEAR 容忍微小精度误差
            EXPECT_NEAR(read_analogs[0], expected_analog, 1e-4) << "Mismatch at row " << rows_read;
            EXPECT_EQ(read_digitals[0], expected_digital) << "Mismatch at row " << rows_read;

            rows_read++;
        }

        // 确保读出的行数与写入的完全一致
        EXPECT_EQ(rows_read, total_rows) << "StreamReader did not read the expected number of rows.";
    }
}

// =========================================================================
// 测试 2：BINARY 格式的流式连续多行读写完整性
// =========================================================================
TEST_F(ComtradeStreamTest, BinaryContinuousStreamRoundTrip) {
    auto cfg = createDummyCfg("BINARY");
    auto base_time = system_clock::now();
    const int total_rows = 500; // Binary 格式测试更大的数据量

    // 1. 测试流式写入
    {
        comtrade::StreamWriter writer(cfg_path, dat_path, cfg);
        for (int i = 0; i < total_rows; ++i) {
            // 测试负数和零等边界值
            double analog_val = -150.25 - i * 0.5;
            uint32_t digital_val = (i % 3 == 0) ? 1 : 0;

            auto ts = base_time + microseconds(i * 250);
            writer.writeRow(ts, {analog_val}, {digital_val});
        }
    }

    // 2. 测试流式读取
    {
        comtrade::StreamReader reader(cfg_path, dat_path);
        EXPECT_EQ(reader.getCfg().data_type, comtrade::DataType::BINARY);

        comtrade::TimePoint read_ts;
        std::vector<double> read_analogs;
        std::vector<uint32_t> read_digitals;

        int rows_read = 0;
        while (reader.readNext(read_ts, read_analogs, read_digitals)) {
            double expected_analog = -150.25 - rows_read * 0.5;
            uint32_t expected_digital = (rows_read % 3 == 0) ? 1 : 0;

            // 二进制格式 (BINARY) 直接读写字节，不应有精度损失，使用 DOUBLE_EQ 严格断言
            EXPECT_DOUBLE_EQ(read_analogs[0], expected_analog);
            EXPECT_EQ(read_digitals[0], expected_digital);

            rows_read++;
        }

        EXPECT_EQ(rows_read, total_rows);
    }
}
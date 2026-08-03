#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <vector>
#include <cstdint>

// 包含你的核心头文件
#include "comtrade/types.hpp"
#include "comtrade/stream_writer.hpp"
#include "comtrade/stream_reader.hpp"

namespace fs = std::filesystem;

// 定义一个 Test Fixture，用于自动管理测试用的临时文件
class ComtradeStreamTest : public ::testing::Test {
protected:
    const std::string cfg_path = "test_stream.cfg";
    const std::string dat_path = "test_stream.dat";

    // 每次 TEST_F 运行前调用
    void SetUp() override {
        cleanUpFiles();
    }

    // 每次 TEST_F 运行后自动调用，保证不留垃圾文件
    void TearDown() override {
        cleanUpFiles();
    }

    void cleanUpFiles() {
        if (fs::exists(cfg_path)) fs::remove(cfg_path);
        if (fs::exists(dat_path)) fs::remove(dat_path);
    }

    // 构造一个模拟的 CfgData 元数据
    comtrade::CfgData createDummyCfg(const std::string& format) {
        comtrade::CfgData cfg;
        cfg.station_name = "Substation_A";
        cfg.id = "Relay_1";
        
        // 1个模拟通道 (Analog) 和 1个数字通道 (Digital)
        // 注意：这里的结构体初始化请根据你实际的 CfgData 字段调整
        cfg.analog_channels.push_back({"Va", "PhaseA", "V", 1.0, 0.0, 0.0, -1000, 1000, 1, 1, "S"});
        cfg.digital_channels.push_back({"CB1_Status", "Breaker", 0});
        
        cfg.file_type = format; // "ASCII" 或 "BINARY"
        return cfg;
    }
};

// 测试 1：ASCII 格式的流式读写完整性 (Round-trip)
TEST_F(ComtradeStreamTest, AsciiRoundTrip) {
    auto cfg = createDummyCfg("ASCII");
    auto now = std::chrono::system_clock::now();
    
    double test_analog_val = 220.5;
    uint32_t test_digital_val = 1;

    // 1. 测试流式写入
    {
        comtrade::StreamWriter writer(cfg_path, dat_path, cfg);
        
        std::vector<double> analogs = {test_analog_val};
        std::vector<uint32_t> digitals = {test_digital_val};
        
        // 假设你的写入 API 类似 writeRow(timestamp, analogs, digitals)
        bool write_ok = writer.writeRow(now, analogs, digitals);
        EXPECT_TRUE(write_ok) << "StreamWriter failed to write ASCII row.";
    } 
    // 离开作用域时，writer 析构，文件流应自动关闭

    // 2. 测试流式读取
    {
        comtrade::StreamReader reader(cfg_path, dat_path);
        
        // 验证元数据解析
        auto read_cfg = reader.getCfg();
        EXPECT_EQ(read_cfg.station_name, "Substation_A");
        EXPECT_EQ(read_cfg.file_type, "ASCII");

        std::vector<double> read_analogs;
        std::vector<uint32_t> read_digitals;
        
        // 假设你的读取 API 类似 readNext(timestamp, analogs, digitals) 返回 bool
        std::chrono::system_clock::time_point read_ts;
        bool has_data = reader.readNext(read_ts, read_analogs, read_digitals);
        
        ASSERT_TRUE(has_data) << "StreamReader failed to read data row.";
        EXPECT_EQ(read_analogs.size(), 1);
        EXPECT_DOUBLE_EQ(read_analogs[0], test_analog_val);
        EXPECT_EQ(read_digitals[0], test_digital_val);
    }
}

// 测试 2：BINARY 格式的流式读写完整性
TEST_F(ComtradeStreamTest, BinaryRoundTrip) {
    auto cfg = createDummyCfg("BINARY");
    auto now = std::chrono::system_clock::now();
    
    // Binary 边界值测试：负数和零
    double test_analog_val = -150.25; 
    uint32_t test_digital_val = 0;

    {
        comtrade::StreamWriter writer(cfg_path, dat_path, cfg);
        writer.writeRow(now, {test_analog_val}, {test_digital_val});
    }

    {
        comtrade::StreamReader reader(cfg_path, dat_path);
        EXPECT_EQ(reader.getCfg().file_type, "BINARY");

        std::chrono::system_clock::time_point read_ts;
        std::vector<double> read_analogs;
        std::vector<uint32_t> read_digitals;
        
        bool has_data = reader.readNext(read_ts, read_analogs, read_digitals);
        ASSERT_TRUE(has_data);
        EXPECT_DOUBLE_EQ(read_analogs[0], test_analog_val);
        EXPECT_EQ(read_digitals[0], test_digital_val);
    }
}
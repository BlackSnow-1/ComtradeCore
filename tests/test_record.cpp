#include <gtest/gtest.h>
#include <comtrade/comtrade.hpp>
#include <fstream>
#include <cstdio> // for std::remove

class ComtradeRecordTest : public ::testing::Test {
protected:
    std::string test_cfg = "test_unified.cfg";
    std::string test_dat = "test_unified.dat";

    // 测试结束后自动清理生成的临时文件
    void TearDown() override {
        std::remove(test_cfg.c_str());
        std::remove(test_dat.c_str());
    }
};

// 测试 1：完整的写出与读入生命周期 (End-to-End)
TEST_F(ComtradeRecordTest, WriteAndParseLifecycle) {
    // ==========================================
    // 阶段 A: 使用 Record 构造数据并落盘
    // ==========================================
    comtrade::Record writer;
    writer.setStationAndDevice("Substation_Alpha", "Diff_Protection");

    // 配置模拟量通道 (变压器差动电流)
    comtrade::AnalogChannel i_diff;
    i_diff.index = 1;
    i_diff.id = "I_diff_A";
    i_diff.a = 0.1; // 乘子为 0.1
    i_diff.b = 0.0;
    writer.addAnalogChannel(i_diff);

    // 配置数字量通道 (电压越限标志)
    comtrade::DigitalChannel v_limit;
    v_limit.index = 1;
    v_limit.id = "Volt_Limit_Flag";
    writer.addDigitalChannel(v_limit);

    // 添加两个采样点
    // 点1: 电流 10.5A, 未越限 (false)
    writer.addSample(0, {10.5}, {false});
    // 点2: 电流 25.0A, 发生越限 (true)
    writer.addSample(1000, {25.0}, {true});

    // 保存文件
    ASSERT_TRUE(writer.saveCfg(test_cfg));
    ASSERT_TRUE(writer.saveDat(test_dat));

    // ==========================================
    // 阶段 B: 使用全新的 Record 实例读取并验证
    // ==========================================
    comtrade::Record reader;
    ASSERT_TRUE(reader.parseCfg(test_cfg));
    ASSERT_TRUE(reader.parseDat(test_dat));

    // 验证配置信息
    EXPECT_EQ(reader.getCfg().station_name, "Substation_Alpha");
    EXPECT_EQ(reader.getCfg().rec_dev_id, "Diff_Protection");
    EXPECT_EQ(reader.getCfg().analog_count, 1);
    EXPECT_EQ(reader.getCfg().digital_count, 1);

    // 验证波形数据 (经过 real -> raw -> real 的转换逻辑)
    const auto& data = reader.getData();
    ASSERT_EQ(data.timestamp.size(), 2);

    // 验证时间戳
    EXPECT_EQ(data.timestamp[0], 0);
    EXPECT_EQ(data.timestamp[1], 1000);

    // 验证模拟量 (10.5 / 0.1 = 105 raw -> 105 * 0.1 = 10.5 real)
    EXPECT_DOUBLE_EQ(data.analog_values[0][0], 10.5);
    EXPECT_DOUBLE_EQ(data.analog_values[0][1], 25.0);

    // 验证数字量
    EXPECT_FALSE(data.digital_values[0][0]);
    EXPECT_TRUE(data.digital_values[0][1]);
}

// 测试 2：内存中数据修改与转储
TEST_F(ComtradeRecordTest, InMemoryManipulation) {
    // 1. 先快速构造并生成一个基础文件
    comtrade::Record origin;
    origin.setStationAndDevice("Original_Station", "Relay_1");

    comtrade::AnalogChannel v_phase;
    v_phase.index = 1;
    v_phase.id = "V_A";
    v_phase.a = 1.0;
    origin.addAnalogChannel(v_phase);
    origin.addSample(0, {220.0}, {});

    ASSERT_TRUE(origin.saveCfg(test_cfg));
    ASSERT_TRUE(origin.saveDat(test_dat));

    // 2. 读取该文件并进行内存修改
    comtrade::Record modifier;
    ASSERT_TRUE(modifier.parseCfg(test_cfg));
    ASSERT_TRUE(modifier.parseDat(test_dat));

    // 修改站名 (使用 getMutableCfg)
    modifier.getMutableCfg().station_name = "Modified_Station";

    // 修改采样值 (将 220.0 注入谐波干扰修改为 250.0)
    modifier.getMutableData().analog_values[0][0] = 250.0;

    // 另存为新文件 (这里为了测试方便直接覆盖原文件)
    ASSERT_TRUE(modifier.saveCfg(test_cfg));
    ASSERT_TRUE(modifier.saveDat(test_dat));

    // 3. 再次读取，验证修改是否生效
    comtrade::Record verifier;
    ASSERT_TRUE(verifier.parseCfg(test_cfg));
    ASSERT_TRUE(verifier.parseDat(test_dat));

    EXPECT_EQ(verifier.getCfg().station_name, "Modified_Station");
    EXPECT_DOUBLE_EQ(verifier.getData().analog_values[0][0], 250.0);
}
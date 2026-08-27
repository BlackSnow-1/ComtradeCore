// 示例目标：绕过逐点接口的容器检查，按通道列批量构造一秒三相波形并写出 ASCII COMTRADE。
// 这种方式适合已知最终样本数的离线生成场景。
#include "comtrade/record.hpp" // 请根据实际包含路径调整
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main() {
    std::cout << "Starting high-performance COMTRADE generation...\n";

    comtrade::Record record;

    // ==========================================
    // 1. 配置基础信息与通道定义
    // ==========================================
    record.setStationAndDevice("Substation_HighVoltage", "Protection_IED_01", comtrade::StandardVersion::V1999);

    // 添加 3 个模拟量通道 (三相电压 Va, Vb, Vc)
    // 假设真实值范围是 -311V 到 +311V，为了落盘为整型，我们设置缩放系数 a = 0.1, b = 0.0
    const std::vector<std::string> phase_names = {"Va", "Vb", "Vc"};
    for (int i = 0; i < 3; ++i) {
        comtrade::AnalogChannel ac;
        ac.index = i + 1;
        ac.id = phase_names[i];
        ac.phase = phase_names[i].substr(1, 1);
        ac.ccbm = "V";
        ac.a = 0.1;
        ac.b = 0.0;
        ac.min = -32767;
        ac.max = 32767;
        record.addAnalogChannel(ac);
    }

    // 添加 1 个数字量通道 (如：保护跳闸信号)
    comtrade::DigitalChannel dc;
    dc.index = 1;
    dc.id = "Trip_Signal";
    dc.normal_state = 0;
    record.addDigitalChannel(dc);

    // ==========================================
    // 2. 高性能批量插入数据 (getMutableData)
    // ==========================================
    int analog_count = record.getCfg().analog_count;
    int digital_count = record.getCfg().digital_count;

    // 设定采样率为 4000Hz (250us 步长)，总时长 1 秒，共 4000 个点
    size_t total_samples = 4000;
    uint32_t step_us = 250;
    double frequency = 50.0; // 50Hz 工频

    // getMutableData 暴露内部列式存储；使用者需要自行保证每列长度与 timestamp 一致。
    auto& data = record.getMutableData();

    // 2.1 预分配内存，避免 vector 频繁扩容
    data.timestamp.reserve(total_samples);

    data.analog_values.resize(analog_count);
    for(int i = 0; i < analog_count; ++i) {
        data.analog_values[i].reserve(total_samples);
    }

    data.digital_values.resize(digital_count);
    for(int i = 0; i < digital_count; ++i) {
        data.digital_values[i].reserve(total_samples);
    }

    // 2.2 极速计算并填入数据 (按列连续存取)
    for (size_t sample_idx = 0; sample_idx < total_samples; ++sample_idx) {
        uint32_t current_time_us = sample_idx * step_us;
        data.timestamp.push_back(current_time_us);

        // 计算当前时间 (秒) 用于生成正弦波
        double t = current_time_us / 1000000.0;

        // 模拟三相电压 (峰值 220 * sqrt(2) ≈ 311V)，相位相差 120 度
        double va = 311.12 * std::sin(2 * M_PI * frequency * t);
        double vb = 311.12 * std::sin(2 * M_PI * frequency * t - 2 * M_PI / 3);
        double vc = 311.12 * std::sin(2 * M_PI * frequency * t + 2 * M_PI / 3);

        data.analog_values[0].push_back(va);
        data.analog_values[1].push_back(vb);
        data.analog_values[2].push_back(vc);

        // 模拟跳闸信号：在 0.5 秒 (sample_idx == 2000) 时刻触发动作
        bool trip_signal = (sample_idx >= 2000);
        data.digital_values[0].push_back(trip_signal);
    }

    // ==========================================
    // 3. 设置时间戳并落盘保存
    // ==========================================
    // 假设录波发生在此刻
    auto now = std::chrono::system_clock::now();
    comtrade::TimePoint start_time = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    comtrade::TimePoint trigger_time = start_time + std::chrono::milliseconds(500); // 0.5秒处触发

    record.setTimestamps(start_time, trigger_time);

    // 强制指定为 ASCII 格式（如果你的类里有这个枚举或配置）
    record.getMutableCfg().data_type = comtrade::DataType::ASCII;

    // saveCfg/saveDat 分开返回状态，实际应用可据此定位配置或数据文件的写入失败。
    std::string cfg_filename = "test_record.cfg";
    std::string dat_filename = "test_record.dat";

    if (record.saveCfg(cfg_filename) && record.saveDat(dat_filename)) {
        std::cout << "[SUCCESS] Successfully generated COMTRADE files:\n"
                  << "  - " << cfg_filename << "\n"
                  << "  - " << dat_filename << "\n"
                  << "Total samples written: " << total_samples << std::endl;
    } else {
        std::cerr << "[ERROR] Failed to save COMTRADE files." << std::endl;
    }

    return 0;
}

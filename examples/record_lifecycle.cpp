#include <comtrade/comtrade.hpp>
#include <chrono>

int main() {
    comtrade::Record record;
    record.setStationAndDevice("Substation_Main", "Diff_Protection");

    // 获取当前时间点 (精确到纳秒)
    auto now = std::chrono::system_clock::now();
    auto start_time = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);

    // 假设触发时间是开始时间之后的 150 毫秒 500 微秒
    auto trigger_time = start_time + std::chrono::milliseconds(150) + std::chrono::microseconds(500);

    // 直接传入强类型时间
    record.setTimestamps(start_time, trigger_time);

    // 文件中将自动生成类似:
    // 03/08/2026,21:14:46.000000000
    // 03/08/2026,21:14:46.150500000
    record.saveCfg("chrono_record.cfg");
    record.saveDat("chrono_record.dat");

    return 0;
}
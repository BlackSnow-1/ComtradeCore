#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include "comtrade/stream_reader.hpp"

// 该程序是轻量级微基准，不依赖 Google Benchmark。应使用 Release 构建比较版本趋势。
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

struct RunResult {
    std::size_t rows = 0;
    double checksum = 0.0;
};

RunResult readOnce(const comtrade::StreamReader& reader, const fs::path& dat_path) {
    RunResult result;
    result.rows = reader.processDatStream(dat_path.string(), [&](const comtrade::SampleRow& row) {
        // 消费少量解析结果，防止优化器把读取循环视为无可观察副作用而删除。
        result.checksum += static_cast<double>(row.index) + static_cast<double>(row.raw_timestamp);
        if (!row.analog_values.empty()) result.checksum += row.analog_values.front();
        if (!row.digital_values.empty()) result.checksum += row.digital_values.front() ? 1.0 : 0.0;
    });
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        fs::path cfg_path = fs::path(COMTRADE_BENCHMARK_DATA_DIR) / "SIMENS/20191024045947.CFG";
        fs::path dat_path = fs::path(COMTRADE_BENCHMARK_DATA_DIR) / "SIMENS/20191024045947.DAT";
        std::size_t iterations = 100;

        if (argc == 3 || argc == 4) {
            cfg_path = argv[1];
            dat_path = argv[2];
            if (argc == 4) iterations = std::stoull(argv[3]);
        } else if (argc != 1) {
            std::cerr << "Usage: " << argv[0] << " [record.cfg record.dat [iterations]]\n";
            return 2;
        }
        if (iterations == 0) throw std::invalid_argument("iterations must be greater than zero");

        const comtrade::StreamReader reader(cfg_path.string());
        // 预热让 CFG、DAT 和解析路径进入操作系统缓存；预热耗时不计入正式结果。
        const auto warmup = readOnce(reader, dat_path);
        if (warmup.rows == 0) throw std::runtime_error("no samples were parsed");

        const auto start = Clock::now();
        std::size_t total_rows = 0;
        double checksum = 0.0;
        for (std::size_t i = 0; i < iterations; ++i) {
            const auto result = readOnce(reader, dat_path);
            if (result.rows != warmup.rows) throw std::runtime_error("sample count changed between iterations");
            total_rows += result.rows;
            checksum += result.checksum;
        }
        const auto elapsed = Clock::now() - start;
        const double seconds = std::chrono::duration<double>(elapsed).count();
        // 吞吐量按 DAT 逻辑读取字节数计算，不包含 CFG，也不等同于物理磁盘带宽。
        const double total_bytes = static_cast<double>(fs::file_size(dat_path)) * iterations;
        const double mib_per_second = total_bytes / (1024.0 * 1024.0) / seconds;
        const double samples_per_second = static_cast<double>(total_rows) / seconds;
        const double nanoseconds_per_sample =
            std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(total_rows);

        std::cout << std::fixed << std::setprecision(3)
                  << "cfg=" << cfg_path.string() << '\n'
                  << "dat=" << dat_path.string() << '\n'
                  << "iterations=" << iterations << '\n'
                  << "samples_per_iteration=" << warmup.rows << '\n'
                  << "elapsed_seconds=" << seconds << '\n'
                  << "throughput_mib_per_second=" << mib_per_second << '\n'
                  << "samples_per_second=" << samples_per_second << '\n'
                  << "nanoseconds_per_sample=" << nanoseconds_per_sample << '\n'
                  << "checksum=" << checksum << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return 1;
    }
}

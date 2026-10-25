#pragma once
#include <comtrade/record.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <chrono>

namespace demo {
namespace fs = std::filesystem;
inline void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
inline void warnings(const comtrade::Warnings& values) {
    for (const auto& w : values)
        std::cout << "Warning " << w.code << ": " << w.message
                  << " (firstRow=" << w.first_row << ", count=" << w.count << ")\n";
}
inline comtrade::Record record(comtrade::DataType type = comtrade::DataType::ASCII) {
    comtrade::Record r;
    r.setStationAndDevice("ExampleStation", "ExampleRelay", comtrade::StandardVersion::V2013);
    auto& cfg = r.getMutableCfg();
    cfg.data_type = type; cfg.time_multiplier = .001;
    cfg.time_code = "+8h00"; cfg.local_code = "+8h00"; cfg.time_quality_code = "0";
    r.setTimestamps("25/10/2026,08:00:00.123456789", "25/10/2026,08:00:00.123956789");
    comtrade::AnalogChannel voltage{};
    voltage.index=1; voltage.id="VA"; voltage.phase="A"; voltage.uu="V";
    voltage.a=.1; voltage.min=-32767; voltage.max=32767; r.addAnalogChannel(voltage);
    auto current=voltage; current.index=2; current.id="IA"; current.uu="A"; current.a=.01; r.addAnalogChannel(current);
    comtrade::DigitalChannel trip{}; trip.index=1; trip.id="TRIP"; r.addDigitalChannel(trip);
    cfg.sample_rates={{4000,5}};
    for (uint64_t i=0;i<5;++i) r.addSample(i*250,{220.0+double(i),5.0+double(i)*.25},{i>=2});
    return r;
}
inline void save(const comtrade::Record& r, const fs::path& stem) {
    require(r.saveCfg(stem.string()+".cfg"), "Cannot save CFG");
    require(r.saveDat(stem.string()+".dat"), "Cannot save DAT");
}
// Each run creates a NEW directory. Existing user files are never overwritten.
template<class Function>
int run(int argc, char** argv, const char* scenario, Function function) {
    try {
        if (argc>2) throw std::runtime_error("Usage: executable [NEW_OUTPUT_DIRECTORY]");
        fs::path output;
        if (argc==2) output=fs::absolute(fs::u8path(argv[1]));
        else output=fs::current_path()/"example-output"/scenario/
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        require(!fs::exists(output), "Output directory already exists; choose a new directory");
        require(fs::create_directories(output), "Cannot create output directory");
        std::cout << "Output: " << output.u8string() << '\n';
        function(output);
        return 0;
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
} // namespace demo

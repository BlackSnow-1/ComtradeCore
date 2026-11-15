#include <comtrade/format_validation.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

static int run(const fs::path& cfg_path, const std::optional<fs::path>& dat_path) {
    std::ifstream cfg(cfg_path, std::ios::binary);
    if (!cfg) {
        std::cerr << "Cannot open CFG: " << cfg_path << '\n';
        return 2;
    }
    std::ifstream dat;
    if (dat_path) {
        dat.open(*dat_path, std::ios::binary);
        if (!dat) {
            std::cerr << "Cannot open DAT: " << *dat_path << '\n';
            return 2;
        }
    }
    auto report = comtrade::checkFormat(cfg, dat_path ? &dat : nullptr);
    if (dat_path && cfg_path.stem() != dat_path->stem())
        comtrade::addWarning(report.warnings, "ieee.pair.filename", "CFG and DAT must have the same base name");
    for (const auto& warning : report.warnings) {
        const auto& path = dat_path && warning.code.find("ieee.dat.") == 0 ? *dat_path : cfg_path;
        comtrade::logWarning(path.u8string(), warning);
    }
    std::cout << (report.ok() ? "PASS" : "FAIL") << " issues=" << report.warnings.size();
    if (dat_path) std::cout << " cfg_samples=" << report.expected_samples << " dat_rows=" << report.actual_samples;
    std::cout << '\n';
    return report.io_error ? 2 : report.ok() ? 0 : 1;
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc >= 2) {
        const fs::path first(argv[1]);
        if (argc == 2 && (first == "--help" || first == "-h")) {
            std::cout << "comtrade-check --cfg FILE.cfg\ncomtrade-check FILE.cfg [FILE.dat]\n";
            return 0;
        }
        if (argc == 3 && first == "--cfg") return run(argv[2], std::nullopt);
        if ((argc == 2 || argc == 3) && first.has_filename()) {
            auto ext = first.extension().string();
            const bool uppercase = ext == ".CFG";
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".cfg") {
                if (argc == 3) return run(first, fs::path(argv[2]));
                auto dat = first;
                dat.replace_extension(uppercase ? ".DAT" : ".dat");
                std::error_code error;
                if (!fs::exists(dat, error)) dat.replace_extension(uppercase ? ".dat" : ".DAT");
                return run(first, dat);
            }
        }
    }
    std::cerr << "Usage: comtrade-check --cfg FILE.cfg | FILE.cfg [FILE.dat]\n";
    return 2;
}

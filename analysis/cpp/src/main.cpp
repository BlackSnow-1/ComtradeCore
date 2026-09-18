#include <comtrade/ai.hpp>
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 6 || (std::string(argv[1]) != "summarize" && std::string(argv[1]) != "analyze")) {
        std::cerr << "Usage: comtrade-ai summarize|analyze config.json input.cfg input.dat "
                     "output.json|output.pdf\n";
        return 2;
    }
    try {
        std::ifstream file(std::filesystem::u8path(argv[2]));
        if (!file)
            throw std::runtime_error("Cannot open config");
        comtrade::ai::Json json;
        try {
            file >> json;
        } catch (...) {
            throw std::runtime_error("Invalid configuration JSON");
        }
        auto config = comtrade::ai::Config::fromJson(json);
        auto output = std::filesystem::u8path(argv[5]);
        if (std::filesystem::exists(output))
            throw std::runtime_error("Output already exists");
        auto evidence = comtrade::ai::summarize(std::filesystem::u8path(argv[3]),
                                                std::filesystem::u8path(argv[4]), config);
        if (std::string(argv[1]) == "analyze") {
            auto text = comtrade::ai::analyze(evidence, config);
            comtrade::ai::exportPdf(output, evidence, text, config);
        } else
            comtrade::ai::writeNewFile(output, evidence.dump(2));
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

// Explicit network example: only run with your own endpoint/configuration and input.
#include <comtrade/ai.hpp>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=5 && argc!=6) {
        std::cerr << "Usage: example_ai_analyze_report config.json input.cff output.pdf --send\n"
                     "       example_ai_analyze_report config.json input.cfg input.dat output.pdf --send\n"; return 2;
    }
    try {
        if(std::string(argv[argc-1])!="--send") throw std::runtime_error("Pass --send to explicitly request a model call");
        const auto config=comtrade::ai::Config::fromFile(std::filesystem::u8path(argv[1]));
        config.validate(true);
        const auto output=std::filesystem::u8path(argv[argc-2]);
        if(std::filesystem::exists(output)) throw std::runtime_error("PDF already exists; choose a new output path");
        const auto parent=output.has_parent_path()?output.parent_path():std::filesystem::current_path();
        if(!std::filesystem::is_directory(parent)) throw std::runtime_error("PDF parent directory does not exist");
        if(!std::filesystem::is_regular_file(std::filesystem::u8path(config.pdfFontPath)))
            throw std::runtime_error("Configure an existing Chinese TTF/TTC font before calling the model");
        auto evidence=argc==5?comtrade::ai::summarizeCff(std::filesystem::u8path(argv[2]),config)
            :comtrade::ai::summarize(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]),config);
        std::cout << "Sending local evidence to the configured model...\n";
        const auto interpretation=comtrade::ai::analyze(evidence,config);
        comtrade::ai::exportPdf(output,evidence,interpretation,config);
        std::cout << "PDF: " << output.u8string() << '\n';
        return 0;
    } catch(const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}

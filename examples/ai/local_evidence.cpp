// Offline measurement evidence: no model, token, network, or PDF font needed.
#include <comtrade/ai.hpp>
#include "../example_support.hpp"
int main(int argc,char** argv) {
    return demo::run(argc,argv,"ai_local_evidence",[](const demo::fs::path& out) {
        auto record=demo::record(comtrade::DataType::FLOAT32); demo::save(record,out/"input");
        demo::require(record.saveCff((out/"input.cff").string()),"Cannot save CFF");
        comtrade::ai::Config config;
        auto summary=comtrade::ai::summarize(out/"input.cfg",out/"input.dat",config);
        config.fullWaveform=true;
        auto full=comtrade::ai::summarizeCff(out/"input.cff",config);
        comtrade::ai::writeNewFile(out/"summary.json",summary.dump(2));
        comtrade::ai::writeNewFile(out/"full-evidence.json",full.dump(2));
        std::cout << "Samples=" << summary.at("samples") << ", schema=" << summary.at("schemaVersion")
                  << ", channels=" << summary.at("channels").size() << '\n';
        // Inspect an exact request locally. This call performs no network I/O.
        config.model="example-model"; config.endpoint="https://example.invalid/v1/chat/completions";
        const auto request=comtrade::ai::makeRequest(summary,config);
        comtrade::ai::writeNewFile(out/"request-preview.json",request);
        std::cout << "Request preview=" << request.size() << " bytes; nothing sent\n";
    });
}

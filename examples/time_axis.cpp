// UTC offsets, irregular/absent DAT timestamps, and multiple sampling rates.
#include "example_support.hpp"
int main(int argc,char** argv) {
    return demo::run(argc,argv,"time_axis",[](const demo::fs::path& out) {
        auto configuration=demo::record();
        configuration.getMutableCfg().sample_rates={{1000,2},{2000,5}};
        demo::require(configuration.saveCfg((out/"rates.cfg").string()),"Cannot save CFG");
        { std::ofstream dat(out/"rates.dat",std::ios::binary);
          dat << "1,0,2200,500,0\r\n2,0,2210,525,0\r\n3,,2220,550,1\r\n4,0,2230,575,1\r\n5,0,2240,600,1\r\n";
          demo::require(bool(dat),"Cannot create example DAT"); }
        for(auto mode:{comtrade::TimeAxisMode::Automatic,comtrade::TimeAxisMode::DatTimestamps,comtrade::TimeAxisMode::SampleRates}) {
            comtrade::ReadOptions options; options.time_axis=mode;
            comtrade::StreamReader reader((out/"rates.cfg").string(),options);
            std::cout << "\nTime axis mode=" << static_cast<int>(mode) << '\n';
            reader.processDatStream((out/"rates.dat").string(),[](const comtrade::SampleRow& row) {
                std::cout << "sample=" << row.index << " offset=" << row.time_offset.count() << " ns, segment="
                          << row.segment_index << " (" << row.segment_sample_rate << " Hz), reconstructed=" << row.timestamp_reconstructed << '\n';
            });
            const auto& cfg=reader.getCfg();
            std::cout << "UTC start=" << comtrade::utils::formatTime(cfg.start_time,9,true)
                      << " (time_code=" << cfg.time_code << ", utcKnown=" << cfg.utc_offset_known << ")\n";
            demo::warnings(reader.getWarnings());
        }
        // Unknown timezone: preserve the local interpretation and inspect cfg.timezone_unknown.
        configuration.getMutableCfg().time_code="x";
        demo::require(configuration.saveCfg((out/"unknown-timezone.cfg").string()),"Cannot save CFG");
        comtrade::StreamReader unknown((out/"unknown-timezone.cfg").string()); demo::warnings(unknown.getWarnings());
    });
}

// Inspect a real recording without loading the full waveform into memory.
#include "example_support.hpp"
#include <algorithm>
#include <iomanip>
int main(int argc,char** argv) {
    try {
        const bool parseToEnd=argc>1 && std::string(argv[argc-1])=="--parse-to-end";
        const int args=argc-(parseToEnd?1:0);
        if(args<2 || args>3) {
            std::cerr << "Usage: example_inspect_record input.cff [--parse-to-end]\n"
                         "       example_inspect_record input.cfg input.dat [--parse-to-end]\n"; return 2;
        }
        comtrade::ReadOptions options;
        if(parseToEnd) options.dat_tail_policy=comtrade::DatTailPolicy::ParseToEnd;
        comtrade::StreamReader reader(argv[1],options);
        demo::require(reader.isCff()?args==2:args==3,"Supply one CFF or a CFG/DAT pair");
        const auto& cfg=reader.getCfg();
        std::cout << cfg.station_name << " / " << cfg.rec_dev_id << ", type=" << comtrade::DataTypeUtils::ToString(cfg.data_type)
                  << ", analog=" << cfg.analog_count << ", digital=" << cfg.digital_count << '\n';
        size_t shown=0,missing=0;
        auto consume=[&](const comtrade::SampleRow& row) {
            for(bool value:row.analog_missing) if(value) ++missing;
            if(shown++<5) {
                std::cout << "Sample " << row.index << ", " << row.time_offset.count() << " ns, analog:";
                for(double value:row.analog_values) std::cout << ' ' << value;
                std::cout << '\n';
            }
        };
        const auto count=reader.isCff()?reader.processCffStream(consume):reader.processDatStream(argv[2],consume);
        std::cout << "Samples=" << count << ", missing analog values=" << missing
                  << ", DAT tail=" << reader.getDatTail().byte_count << " bytes\n";
        demo::warnings(reader.getWarnings());
        return count?0:1;
    } catch(const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}

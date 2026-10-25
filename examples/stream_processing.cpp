// Generate and process 20,000 samples with constant waveform memory.
#include "example_support.hpp"
#include <comtrade/stream_reader.hpp>
#include <array>
int main(int argc,char** argv) {
    return demo::run(argc,argv,"stream_processing",[](const demo::fs::path& out) {
        auto configuration=demo::record(comtrade::DataType::BINARY);
        configuration.getMutableData()={};
        // A 5-second binary record needs microsecond ticks: nanosecond ticks exceed uint32.
        configuration.getMutableCfg().time_multiplier=1;
        constexpr uint32_t count=20000;
        configuration.getMutableCfg().sample_rates={{4000,count}};
        demo::require(configuration.saveCfg((out/"stream.cfg").string()),"Cannot save CFG");
        comtrade::StreamWriter writer(configuration.getCfg());
        demo::require(writer.open((out/"stream.dat").string()),"Cannot open DAT");
        std::vector<double> analog(2); std::vector<bool> digital(1);
        for(uint32_t i=0;i<count;++i) {
            analog[0]=220.0+std::sin(i*.05); analog[1]=5.0; digital[0]=i>=10000;
            writer.pushRow(uint64_t(i)*250,analog,digital);
        }
        writer.close();
        comtrade::StreamReader reader((out/"stream.cfg").string());
        double sum=0; size_t tripCount=0;
        const auto processed=reader.processDatStream((out/"stream.dat").string(),[&](const comtrade::SampleRow& row) {
            sum+=row.analog_values[0]; tripCount+=row.digital_values[0]?1:0;
            if(row.index<=3) std::cout << row.index << ": " << row.timestamp_us << " us, " << row.analog_values[0] << " V\n";
            // row and its containers are reused: copy if retaining data after this callback.
        });
        demo::require(processed==count,"Incomplete stream");
        std::cout << "Samples=" << processed << ", mean voltage=" << sum/processed << ", trip=true at " << tripCount << " samples\n";
        demo::warnings(reader.getWarnings());
    });
}

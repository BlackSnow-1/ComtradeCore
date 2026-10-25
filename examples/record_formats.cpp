// Complete in-memory read/edit/save, with all four DAT encodings.
#include "example_support.hpp"
int main(int argc,char** argv) {
    return demo::run(argc,argv,"record_formats",[](const demo::fs::path& out) {
        for(auto type:{comtrade::DataType::ASCII,comtrade::DataType::BINARY,comtrade::DataType::BINARY32,comtrade::DataType::FLOAT32}) {
            const auto name=comtrade::DataTypeUtils::ToString(type);
            auto original=demo::record(type); demo::save(original,out/name);
            comtrade::Record loaded;
            demo::require(loaded.parseCfg((out/(name+".cfg")).string()),"Cannot parse CFG");
            demo::require(loaded.parseDat((out/(name+".dat")).string()),"Cannot parse DAT");
            demo::require(loaded.getData().timestamp.size()==5,"Incomplete record");
            std::cout << name << ": first voltage=" << loaded.getData().analog_values[0][0]
                      << " V, samples=" << loaded.getData().timestamp.size() << '\n';
            // Values in memory are engineering values, not raw ADC integers.
            loaded.getMutableData().analog_values[0][0]=230.0;
            loaded.getMutableCfg().station_name="EditedStation";
            demo::save(loaded,out/(name+"_edited")); demo::warnings(loaded.getWarnings());
        }
    });
}

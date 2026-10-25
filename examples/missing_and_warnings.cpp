// Missing analog values stay as NaN, malformed rows produce aggregated warnings.
#include "example_support.hpp"
#include <limits>
int main(int argc,char** argv) {
    return demo::run(argc,argv,"missing_and_warnings",[](const demo::fs::path& out) {
        for(auto type:{comtrade::DataType::ASCII,comtrade::DataType::BINARY,comtrade::DataType::BINARY32,comtrade::DataType::FLOAT32}) {
            const auto name=comtrade::DataTypeUtils::ToString(type); auto record=demo::record(type);
            record.getMutableData().analog_values[0][2]=std::numeric_limits<double>::quiet_NaN();
            demo::save(record,out/name);
            comtrade::StreamReader reader((out/(name+".cfg")).string()); size_t missing=0;
            reader.processDatStream((out/(name+".dat")).string(),[&](const comtrade::SampleRow& row) {
                if(row.analog_missing[0]) { ++missing; demo::require(std::isnan(row.analog_values[0]),"Missing marker not NaN"); }
            });
            demo::require(missing==1,"Missing sample not preserved");
            std::cout << name << ": missing=" << missing << '\n'; demo::warnings(reader.getWarnings());
        }
        { std::ofstream dat(out/"damaged.dat",std::ios::binary);
          dat << "1,0,2200,500,0\nbad-row\n3,500000,99999,550,2\n4,750000,2230,575,1\n5,1000000,2240,600,1\n";
          demo::require(bool(dat),"Cannot create damaged example"); }
        comtrade::StreamReader reader((out/"ASCII.cfg").string());
        const auto count=reader.processDatStream((out/"damaged.dat").string(),[](const comtrade::SampleRow&){});
        std::cout << "Recovered " << count << " valid rows from damaged DAT\n";
        demo::require(count==4,"Unexpected recovery count"); demo::warnings(reader.getWarnings());
    });
}

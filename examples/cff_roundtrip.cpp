// CFF metadata, four DAT types, and sub-microsecond time precision.
#include "example_support.hpp"
int main(int argc,char** argv) {
    return demo::run(argc,argv,"cff_roundtrip",[](const demo::fs::path& out) {
        for(auto type:{comtrade::DataType::ASCII,comtrade::DataType::BINARY,comtrade::DataType::BINARY32,comtrade::DataType::FLOAT32}) {
            const auto name=comtrade::DataTypeUtils::ToString(type);
            auto original=demo::record(type);
            original.setHeader("Operator: example\nEvent: demonstration\n");
            original.setInformation("[vendor]\nsource=example\n");
            // Keep the public microsecond and nanosecond columns consistent.
            original.getMutableData().timestamp[1]=0;
            original.getMutableData().time_offset_ns[1]=125;
            const auto path=out/(name+".cff");
            demo::require(original.saveCff(path.string()),"Cannot save CFF");
            comtrade::Record loaded; demo::require(loaded.loadCff(path.string()),"Cannot load CFF");
            demo::require(loaded.getData().time_offset_ns[1]==125,"Nanosecond precision lost");
            std::cout << name << " HDR:\n" << loaded.getHeader() << "INF:\n" << loaded.getInformation();
            comtrade::StreamReader stream(path.string());
            const auto count=stream.processCffStream([](const comtrade::SampleRow& row) {
                if(row.index==2) std::cout << "Sample 2 offset=" << row.time_offset.count() << " ns\n";
            });
            demo::require(count==5,"Incomplete CFF stream");
            loaded.setHeader("Operator notes updated\n");
            demo::require(loaded.saveCff((out/(name+"_edited.cff")).string()),"Cannot save edited CFF");
            demo::warnings(stream.getWarnings());
        }
    });
}

// Opaque trailer extraction, bounded memory, explicit copying, and an incorrect CFG count.
#include "example_support.hpp"
int main(int argc,char** argv) {
    return demo::run(argc,argv,"vendor_tail",[](const demo::fs::path& out) {
        const std::string footer("VENDOR\0\xff\r\n",10);
        for(auto type:{comtrade::DataType::ASCII,comtrade::DataType::BINARY,comtrade::DataType::BINARY32,comtrade::DataType::FLOAT32}) {
            const auto name=comtrade::DataTypeUtils::ToString(type); const auto stem=out/name;
            auto record=demo::record(type); demo::save(record,stem);
            { std::ofstream dat(stem.string()+".dat",std::ios::binary|std::ios::app); dat.write(footer.data(),footer.size()); demo::require(bool(dat),"Cannot append example tail"); }
            comtrade::ReadOptions options; options.max_retained_tail_bytes=4;
            comtrade::StreamReader reader(stem.string()+".cfg",options);
            std::ofstream raw(out/(name+"_stream-tail.bin"),std::ios::binary);
            reader.setDatTailHandler([&](const char* bytes,size_t size,uint64_t offset) {
                std::cout << name << " tail chunk: offset=" << offset << ", bytes=" << size << '\n';
                raw.write(bytes,static_cast<std::streamsize>(size)); demo::require(bool(raw),"Tail export failed");
            });
            demo::require(reader.processDatStream(stem.string()+".dat",[](const comtrade::SampleRow&){})==5,"Trailer was decoded as waveform");
            raw.close(); demo::require(!raw.fail(),"Cannot close tail export");
            const auto& tail=reader.getDatTail();
            std::cout << "Tail size=" << tail.byte_count << ", preview=" << tail.bytes.size() << ", truncated=" << tail.truncated << '\n';
            demo::warnings(reader.getWarnings());
            comtrade::Record loaded;
            demo::require(loaded.parseCfg(stem.string()+".cfg") && loaded.parseDat(stem.string()+".dat"),"Cannot load recording");
            demo::require(loaded.getDatTail().bytes==footer,"Raw trailer bytes changed");
            demo::require(loaded.saveDatTail((out/(name+"_tail.bin")).string()),"Cannot export complete tail");
            demo::require(loaded.saveDat((out/(name+"_standard.dat")).string()),"Cannot save standard DAT");
            // Explicit copy does NOT update vendor checksums/offsets after waveform serialization.
            demo::require(loaded.saveDat((out/(name+"_copied.dat")).string(),true),"Cannot copy trailer");
            demo::warnings(loaded.getWarnings());
        }
        auto wrong=demo::record(); wrong.getMutableCfg().sample_rates={{4000,2}};
        demo::save(wrong,out/"short-count");
        for(auto policy:{comtrade::DatTailPolicy::AfterCfgSamples,comtrade::DatTailPolicy::ParseToEnd}) {
            comtrade::ReadOptions options; options.dat_tail_policy=policy;
            comtrade::StreamReader reader((out/"short-count.cfg").string(),options);
            const auto n=reader.processDatStream((out/"short-count.dat").string(),[](const comtrade::SampleRow&){});
            std::cout << "Policy=" << static_cast<int>(policy) << ": samples=" << n << ", tail=" << reader.getDatTail().byte_count << " bytes\n";
            demo::warnings(reader.getWarnings());
        }
    });
}

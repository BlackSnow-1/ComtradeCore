#include <gtest/gtest.h>
#include <comtrade/record.hpp>
#include <sstream>
#include <filesystem>
#include <fstream>
using namespace comtrade;
namespace {
CfgData cfg(DataType type) {
    CfgData c; c.version=StandardVersion::V2013; c.data_type=type;
    c.station_name="S"; c.rec_dev_id="D"; c.analog_count=1; c.total_channels=1;
    AnalogChannel a{}; a.index=1; a.id="A"; c.analog_channels.push_back(a);
    c.sample_rates={{1000,2}}; return c;
}
std::string samples(const CfgData& c) {
    std::ostringstream out; StreamWriter writer(c); writer.open(out);
    writer.pushRow(0,{1},{}); writer.pushRow(1000,{2},{}); writer.close(); return out.str();
}
bool warned(const Warnings& warnings,const std::string& code) {
    for(const auto& w:warnings) if(w.code==code) return true; return false;
}
TEST(DatTail, FourEncodingsKeepOpaqueFooterOutOfWaveform) {
    const std::string tail("VENDOR\0\xff\r\n1,99,7\r\n", sizeof("VENDOR\0\xff\r\n1,99,7\r\n")-1);
    for(auto type:{DataType::ASCII,DataType::BINARY,DataType::BINARY32,DataType::FLOAT32}) {
        auto c=cfg(type); const auto standard=samples(c); std::istringstream in(standard+tail);
        StreamReader reader(c); size_t count=0;
        EXPECT_EQ(reader.processStream(in,[&](const SampleRow& row){++count; EXPECT_EQ(row.analog_values[0],double(count));}),2u);
        EXPECT_EQ(reader.getPhysicalRowCount(),2u); EXPECT_EQ(reader.getDatTail().offset,standard.size());
        EXPECT_EQ(reader.getDatTail().bytes,tail); EXPECT_EQ(reader.getDatTail().byte_count,tail.size());
        EXPECT_TRUE(warned(reader.getWarnings(),"dat.vendor_tail")); EXPECT_FALSE(warned(reader.getWarnings(),"dat.partial_row"));
    }
}
TEST(DatTail, FullBinaryRecordSizedFooterIsNotDecoded) {
    const auto c=cfg(DataType::BINARY); std::string footer(10,'\0'); char* p=footer.data();
    detail::writeUint32LittleEndian(p,3); detail::writeUint32LittleEndian(p,2000); detail::writeInt16LittleEndian(p,99);
    std::istringstream in(samples(c)+footer); StreamReader reader(c);
    EXPECT_EQ(reader.processStream(in,[](const auto&){}),2u); EXPECT_EQ(reader.getDatTail().bytes,footer);
}
TEST(DatTail, IncorrectCfgCountHasAnExplicitParseToEndEscapeHatch) {
    auto c=cfg(DataType::ASCII); c.sample_rates[0].end_sample=1;
    ReadOptions options; options.dat_tail_policy=DatTailPolicy::ParseToEnd;
    StreamReader reader(c,options); std::istringstream in(samples(c));
    EXPECT_EQ(reader.processStream(in,[](const auto&){}),2u); EXPECT_EQ(reader.getDatTail().byte_count,0u);
    EXPECT_TRUE(warned(reader.getWarnings(),"dat.sample_count"));
}
TEST(DatTail, RetentionLimitStillStreamsAllBytesAndResetsEachRun) {
    const auto c=cfg(DataType::BINARY); const auto standard=samples(c); std::string tail(150000,'X'); tail[70000]='\0';
    ReadOptions options; options.max_retained_tail_bytes=7; StreamReader reader(c,options);
    std::string captured;
    reader.setDatTailHandler([&](const char* bytes,size_t length,uint64_t offset){EXPECT_EQ(offset,standard.size()+captured.size());captured.append(bytes,length);});
    std::istringstream in(standard+tail); EXPECT_EQ(reader.processStream(in,[](const auto&){}),2u);
    EXPECT_EQ(captured,tail); EXPECT_EQ(reader.getDatTail().bytes,tail.substr(0,7)); EXPECT_TRUE(reader.getDatTail().truncated);
    EXPECT_TRUE(warned(reader.getWarnings(),"dat.vendor_tail_limit"));
    std::istringstream clean(standard); reader.processStream(clean,[](const auto&){});
    EXPECT_EQ(reader.getDatTail().byte_count,0u); EXPECT_FALSE(reader.getDatTail().truncated);
}
TEST(DatTail, CffByteLimitDoesNotLeakIntoAnOuterTrailer) {
    const auto c=cfg(DataType::BINARY); const auto standard=samples(c);
    std::istringstream in(standard+"INNEROUTER"); StreamReader reader(c);
    EXPECT_EQ(reader.processStream(in,[](const auto&){},standard.size()+5),2u);
    EXPECT_EQ(reader.getDatTail().bytes,"INNER"); EXPECT_EQ(in.get(),'O');
}
TEST(DatTail, CallbackExceptionsPropagate) {
    const auto c=cfg(DataType::BINARY); StreamReader reader(c);
    reader.setDatTailHandler([](const char*,size_t,uint64_t){throw std::runtime_error("tail callback");});
    std::istringstream in(samples(c)+"TAIL"); EXPECT_THROW(reader.processStream(in,[](const auto&){}),std::runtime_error);
}
TEST(DatTail, RecordExportsAndExplicitlyCopiesFooterWithoutClaimingToUpdateChecksums) {
    const auto c=cfg(DataType::BINARY); const auto standard=samples(c); const std::string footer("A\0B",3);
    const auto path=std::filesystem::current_path()/"dat-tail-test.dat";
    {std::ofstream file(path,std::ios::binary); file<<standard<<footer;}
    Record record; record.getMutableCfg()=c; ASSERT_TRUE(record.parseDat(path.string()));
    EXPECT_EQ(record.getDatTail().bytes,footer);
    std::ostringstream normalized; ASSERT_TRUE(record.writeDat(normalized)); EXPECT_EQ(normalized.str(),standard);
    EXPECT_TRUE(warned(record.getWarnings(),"dat.vendor_tail_omitted"));
    std::ostringstream copied; ASSERT_TRUE(record.writeDat(copied,true)); EXPECT_EQ(copied.str(),standard+footer);
    EXPECT_TRUE(warned(record.getWarnings(),"dat.vendor_tail_copied"));
    ASSERT_TRUE(record.saveDatTail(path.string()));
    std::ifstream file(path,std::ios::binary); const std::string exported{std::istreambuf_iterator<char>(file),{}};
    EXPECT_EQ(exported,footer); file.close(); std::filesystem::remove(path);
}
TEST(DatTail, UnknownBoundaryKeepsLegacyParsingAndDoesNotInventAFooter) {
    auto c=cfg(DataType::BINARY); c.sample_rates.clear(); StreamReader reader(c);
    std::istringstream in(samples(c)+"x"); EXPECT_EQ(reader.processStream(in,[](const auto&){}),2u);
    EXPECT_EQ(reader.getDatTail().byte_count,0u); EXPECT_TRUE(warned(reader.getWarnings(),"dat.partial_row"));
}
TEST(DatTail, IncompleteStandardRecordIsStillReportedAsDamage) {
    const auto c=cfg(DataType::BINARY); auto data=samples(c); data.pop_back();
    std::istringstream in(data); StreamReader reader(c);
    EXPECT_EQ(reader.processStream(in,[](const auto&){}),1u); EXPECT_EQ(reader.getDatTail().byte_count,0u);
    EXPECT_TRUE(warned(reader.getWarnings(),"dat.partial_row"));
}
}

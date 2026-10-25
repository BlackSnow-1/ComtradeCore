#include <gtest/gtest.h>
#include <comtrade/record.hpp>
#include <sstream>
#include <filesystem>
#include <fstream>
using namespace comtrade;
namespace {
CfgData configuration(DataType type = DataType::ASCII) {
    CfgData c; c.version = StandardVersion::V2013; c.data_type = type;
    c.station_name="S"; c.rec_dev_id="R"; c.total_channels=1; c.analog_count=1;
    AnalogChannel a{}; a.index=1; a.id="A"; a.a=2; a.b=3; c.analog_channels.push_back(a);
    c.sample_rates={{1000,2},{2000,4}}; return c;
}
bool warned(const Warnings& ws, const std::string& code) {
    for (const auto& w:ws) if(w.code==code) return true; return false;
}
TEST(Extensions, CffRoundTripsAllEncodingsMetadataAndNanoseconds) {
    const auto path=std::filesystem::current_path()/"extensions.cff";
    for(auto type:{DataType::ASCII,DataType::BINARY,DataType::BINARY32,DataType::FLOAT32}) {
        Record r; r.getMutableCfg()=configuration(type); r.getMutableCfg().time_multiplier=.001;
        r.setHeader("operator notes\n"); r.setInformation("[vendor]\nkey=value\n");
        r.addSample(0,{5},{}); r.addSample(0,{std::numeric_limits<double>::quiet_NaN()},{});
        r.getMutableData().time_offset_ns={0,125};
        ASSERT_TRUE(r.saveCff(path.string()));
        Record loaded; ASSERT_TRUE(loaded.loadCff(path.string()));
        EXPECT_EQ(loaded.getHeader(),r.getHeader()); EXPECT_EQ(loaded.getInformation(),r.getInformation());
        ASSERT_EQ(loaded.getData().timestamp.size(),2u);
        EXPECT_EQ(loaded.getData().time_offset_ns[1],125); EXPECT_TRUE(std::isnan(loaded.getData().analog_values[0][1]));
        StreamReader reader(path.string()); size_t n=0;
        EXPECT_EQ(reader.processCffStream([&](const SampleRow& row){if(n++==1) EXPECT_TRUE(row.analog_missing[0]);}),2u);
    }
    std::filesystem::remove(path);
}
TEST(Extensions, MultiRateReconstructionUsesSampleNumbersAcrossBadRows) {
    StreamReader reader(configuration()); std::istringstream dat("1,0,1\nBAD\n3,,2\n4,0,3\n");
    std::vector<SampleRow> rows; EXPECT_EQ(reader.processStream(dat,[&](const auto& r){rows.push_back(r);}),3u);
    EXPECT_EQ(rows[1].time_offset.count(),1500000); EXPECT_EQ(rows[2].time_offset.count(),2000000);
    EXPECT_EQ(rows[1].segment_index,1u); EXPECT_TRUE(rows[1].timestamp_reconstructed);
    EXPECT_TRUE(warned(reader.getWarnings(),"dat.bad_row")); EXPECT_TRUE(warned(reader.getWarnings(),"dat.index"));
}
TEST(Extensions, TimeAxisPolicyCanPreserveRawZeros) {
    ReadOptions opts; opts.time_axis=TimeAxisMode::DatTimestamps;
    StreamReader reader(configuration(),opts); std::istringstream dat("1,0,1\n2,0,2\n");
    reader.processStream(dat,[](const auto& row){EXPECT_EQ(row.time_offset.count(),0);});
    EXPECT_TRUE(warned(reader.getWarnings(),"dat.time_order"));
}
TEST(Extensions, TimeOffsetsUseTimeCodeRatherThanLocalCode) {
    Record r; r.getMutableCfg()=configuration(); r.getMutableCfg().time_code="+8h30";
    r.getMutableCfg().local_code="-5"; r.setTimestamps("01/01/2020,08:30:00.123456789","01/01/2020,08:30:01.123456789");
    EXPECT_EQ(r.getCfg().start_time,utils::parseTime("01/01/2020,00:00:00.123456789",true));
    std::ostringstream out; ASSERT_TRUE(r.writeCfg(out));
    EXPECT_NE(out.str().find("01/01/2020,08:30:00.123456789"),std::string::npos);
    std::vector<std::string> lines; std::istringstream input(out.str()); std::string line;
    while(std::getline(input,line)) lines.push_back(line);
    CfgData parsed; ASSERT_TRUE(detail::parseCfgLines(lines,parsed));
    EXPECT_EQ(parsed.start_time,r.getCfg().start_time); EXPECT_TRUE(parsed.utc_offset_known);
}
TEST(Extensions, MissingValuesRetainRowsAndFollowingColumns) {
    StreamReader reader(configuration()); std::istringstream dat("1,0,99999\n2,1000,\n3,1500,2\n");
    std::vector<SampleRow> rows; EXPECT_EQ(reader.processStream(dat,[&](const auto& r){rows.push_back(r);}),3u);
    EXPECT_TRUE(std::isnan(rows[0].analog_values[0])); EXPECT_TRUE(std::isnan(rows[1].analog_values[0]));
    EXPECT_EQ(rows[2].analog_values[0],7); EXPECT_TRUE(warned(reader.getWarnings(),"dat.missing_analog"));
}
TEST(Extensions, LegacyMissingMarkersAreRevisionDependent) {
    auto c=configuration(DataType::BINARY); c.version=StandardVersion::V1991;
    StreamReader reader(c); std::string bytes(12,'\0'); char* cursor=bytes.data();
    detail::writeUint32LittleEndian(cursor,1); detail::writeUint32LittleEndian(cursor,0); detail::writeInt16LittleEndian(cursor,-1);
    bytes.resize(10); std::istringstream input(bytes);
    EXPECT_EQ(reader.processStream(input,[](const auto& r){EXPECT_TRUE(std::isnan(r.analog_values[0]));}),1u);
}
TEST(Extensions, RecordPreservesLongTimeAndSharesDecoder) {
    Record r; r.getMutableCfg()=configuration(); r.addSample(4294967296ULL,{5},{});
    std::ostringstream out; ASSERT_TRUE(r.writeDat(out)); EXPECT_NE(out.str().find("4294967296"),std::string::npos);
    std::istringstream in(out.str()); StreamReader reader(r.getCfg());
    reader.processStream(in,[](const auto& row){EXPECT_EQ(row.timestamp_us,4294967296ULL);});
}
TEST(Extensions, WarningsAreAggregatedAndResetEachRun) {
    StreamReader reader(configuration()); std::istringstream bad("bad\nbad\nbad\n");
    reader.processStream(bad,[](const auto&){}); uint64_t count=0;
    for(const auto& w:reader.getWarnings()) if(w.code=="dat.bad_row") count=w.count;
    EXPECT_EQ(count,3u); std::istringstream good("1,0,1\n"); reader.processStream(good,[](const auto&){});
    EXPECT_FALSE(warned(reader.getWarnings(),"dat.bad_row"));
}
TEST(Extensions, PartialBinaryWarnsAndKeepsCompleteRows) {
    auto c=configuration(DataType::BINARY); std::ostringstream out; StreamWriter writer(c); writer.open(out); writer.pushRow(0,{5},{}); writer.close();
    std::istringstream in(out.str()+"x"); StreamReader reader(c);
    EXPECT_EQ(reader.processStream(in,[](const auto&){}),1u); EXPECT_TRUE(warned(reader.getWarnings(),"dat.partial_row"));
}
TEST(Extensions, CffDoesNotScanBinaryPayloadAsText) {
    Record r; r.getMutableCfg()=configuration(DataType::BINARY32); std::ostringstream cfg; ASSERT_TRUE(r.writeCfg(cfg));
    std::string data="--- file type: CFG ---\r\n"+cfg.str()+"--- file type: DAT BINARY:1 ---\r\nxTRAILING";
    std::istringstream in(data); detail::CffSections parts; ASSERT_TRUE(detail::readCff(in,parts)); EXPECT_EQ(parts.dat_bytes,1u);
    EXPECT_EQ(in.get(),'x');
}
TEST(Extensions, CfgRecoversCountsAndOptionalMetadataButRejectsInvalidDates) {
    Record r; r.getMutableCfg()=configuration(); std::ostringstream out; ASSERT_TRUE(r.writeCfg(out));
    std::string text=out.str(); text.replace(text.find("1,1A,0D"),8,"9,1A,0D");
    std::vector<std::string> lines; std::istringstream in(text); std::string line; while(std::getline(in,line)) lines.push_back(line);
    CfgData cfg; ASSERT_TRUE(detail::parseCfgLines(lines,cfg)); EXPECT_EQ(cfg.total_channels,1);
    EXPECT_TRUE(warned(cfg.warnings,"cfg.channel_count"));
    EXPECT_THROW(utils::parseTime("31/02/2020,00:00:00"),std::invalid_argument);
}
TEST(Extensions, WriterDetectsOutputFailure) {
    StreamWriter writer(configuration()); std::ostringstream out; writer.open(out); out.setstate(std::ios::badbit);
    EXPECT_THROW(writer.pushRow(0,{5},{}),std::runtime_error);
}
TEST(Extensions, AppendingAfterLoadPreservesSubMicrosecondTimes) {
    Record r; r.getMutableCfg()=configuration();
    r.addSample(0,{5},{}); r.getMutableData().time_offset_ns[0]=125;
    r.addSample(1000,{7},{});
    EXPECT_EQ(r.getData().time_offset_ns[0],125); EXPECT_EQ(r.getData().time_offset_ns.size(),2u);
    EXPECT_THROW(r.addSample(2000,{},{}),std::invalid_argument);
}
TEST(Extensions, OptionalInvalidNumbersRecoverWithWarnings) {
    Record r; r.getMutableCfg()=configuration(); std::ostringstream output; ASSERT_TRUE(r.writeCfg(output));
    auto text=output.str(); auto position=text.find(",0,0,0,1,1,P");
    ASSERT_NE(position,std::string::npos); text.replace(position,2,",bad");
    std::vector<std::string> lines; std::istringstream in(text); std::string line;
    while(std::getline(in,line)) lines.push_back(line);
    CfgData cfg; ASSERT_TRUE(detail::parseCfgLines(lines,cfg));
    EXPECT_EQ(cfg.analog_channels[0].skew,0); EXPECT_TRUE(warned(cfg.warnings,"cfg.optional_default"));
}
TEST(Extensions, PreEpochTimesKeepPositiveFraction) {
    const auto time=utils::parseTime("31/12/1969,23:59:59.123456789",true);
    EXPECT_EQ(utils::formatTime(time,9,true),"31/12/1969,23:59:59.123456789");
}

}

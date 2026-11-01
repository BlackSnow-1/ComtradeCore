#include <gtest/gtest.h>
#include <comtrade/stream_reader.hpp>
#include <sstream>

namespace {
std::vector<std::string> validCfg() {
    return {"S,R,2013", "2,1A,1D", "1,VA,A,,V,2,3,0,-100,100,1,1,P",
            "1,TRIP,,,0", "50", "1", "1000,3", "01/01/2020,00:00:00.000000000",
            "01/01/2020,00:00:00.000000000", "ASCII", "1", "+0,+0", "0,0"};
}
bool has(const comtrade::CfgData& cfg, const std::string& code) {
    for (const auto& w : cfg.warnings) if (w.code == code) return true;
    return false;
}
TEST(CfgValidation, RecoverableNumbersDefaultWithoutAcceptingPrefixesOrFractions) {
    auto lines = validCfg();
    lines[1] = "garbage,1a,1";
    lines[2] = "1abc,VA,A,,V,2,3,7bad,-100,100,1,1,P";
    lines[3] = "1.5,TRIP,,,0.5";
    lines[4] = "50Hz"; lines[10] = "2junk"; lines[12] = "0,1junk";
    comtrade::CfgData cfg;
    ASSERT_TRUE(comtrade::detail::parseCfgLines(lines, cfg));
    EXPECT_EQ(cfg.total_channels, 2); EXPECT_EQ(cfg.analog_channels[0].index, 1);
    EXPECT_EQ(cfg.digital_channels[0].index, 1); EXPECT_EQ(cfg.digital_channels[0].normal_state, 0);
    EXPECT_EQ(cfg.analog_channels[0].skew, 0); EXPECT_DOUBLE_EQ(cfg.line_frequency, 50);
    EXPECT_DOUBLE_EQ(cfg.time_multiplier, 1); EXPECT_EQ(cfg.leap_second, 0);
    for (const auto* code : {"cfg.total_default", "cfg.channel_suffix", "cfg.optional_default",
                             "cfg.frequency", "cfg.timemult", "cfg.leap_second"}) EXPECT_TRUE(has(cfg, code)) << code;
}
TEST(CfgValidation, StructuralCountsAndScalingNeverAcceptMalformedNumericPrefixes) {
    for (const auto& mutation : std::vector<std::pair<size_t,std::string>>{
        {1,"2,1AX,1D"}, {1,"2,-1A,1D"}, {1,"2,2147483648A,1D"},
        {2,"1,VA,A,,V,2junk,3,0,-100,100,1,1,P"},
        {2,"1,VA,A,,V,nan,3,0,-100,100,1,1,P"}, {5,"1tail"}, {5,"1.5"}, {6,"1000,3junk"}}) {
        auto lines=validCfg(); lines[mutation.first]=mutation.second;
        comtrade::CfgData cfg; cfg.station_name="unchanged";
        bool accepted=false;
        try { accepted=comtrade::detail::parseCfgLines(lines,cfg); } catch(const std::exception&) {}
        EXPECT_FALSE(accepted) << mutation.second;
        EXPECT_EQ(cfg.station_name,"unchanged");
    }
}
TEST(CfgValidation, SuspiciousDigitalIndicesAndLocalCodesArePreservedWithWarnings) {
    auto lines=validCfg(); lines[3]="9,TRIP,,,2"; lines[11]="+0,bad";
    comtrade::CfgData cfg; ASSERT_TRUE(comtrade::detail::parseCfgLines(lines,cfg));
    EXPECT_EQ(cfg.digital_channels[0].index,9); EXPECT_EQ(cfg.digital_channels[0].normal_state,2);
    EXPECT_TRUE(has(cfg,"cfg.channel_index")); EXPECT_TRUE(has(cfg,"cfg.digital_state"));
    EXPECT_TRUE(has(cfg,"cfg.local_code")); EXPECT_TRUE(cfg.utc_offset_known);
}
TEST(CfgValidation, WhitespaceIsAcceptedAndValidEngineeringCoefficientsArePreserved) {
    auto lines=validCfg(); lines[1]=" 2 , 1A , 1D "; lines[2]="1,VA,A,,V,\t-2e-1\t,+3.5,0,-100,100,1,1,P";
    lines[4]="\t50\t"; lines[10]=" 0.001 ";
    comtrade::CfgData cfg; ASSERT_TRUE(comtrade::detail::parseCfgLines(lines,cfg));
    EXPECT_DOUBLE_EQ(cfg.analog_channels[0].a,-0.2); EXPECT_DOUBLE_EQ(cfg.analog_channels[0].b,3.5);
    EXPECT_DOUBLE_EQ(cfg.time_multiplier,0.001); EXPECT_FALSE(has(cfg,"cfg.optional_default"));
}
TEST(CfgValidation, ExtraFieldsAreIgnoredWithAggregatedLocatedWarnings) {
    auto lines=validCfg(); lines[0]+=",vendor"; lines[3]+=",vendor"; lines[6]+=",vendor";
    comtrade::CfgData cfg; ASSERT_TRUE(comtrade::detail::parseCfgLines(lines,cfg));
    EXPECT_EQ(cfg.digital_channels[0].normal_state,0); EXPECT_EQ(cfg.sample_rates[0].end_sample,3u);
    bool found=false;
    for(const auto& w:cfg.warnings) if(w.code=="cfg.extra_columns") {
        found=true; EXPECT_EQ(w.first_row,1u); EXPECT_EQ(w.count,3u);
    }
    EXPECT_TRUE(found);
}
TEST(CfgValidation, DatSkipsInvalidIndicesAndDigitalTokensButKeepsNonbooleanSignedStates) {
    comtrade::CfgData cfg; ASSERT_TRUE(comtrade::detail::parseCfgLines(validCfg(),cfg));
    comtrade::ReadOptions options; options.dat_tail_policy=comtrade::DatTailPolicy::ParseToEnd;
    comtrade::StreamReader reader(cfg, options);
    std::istringstream dat("0,0,1,0\n1,0,1,-1\n2,1000,1,0.5\n2,1000,1,2junk\n2,1000,1,1\n3,2000,1,0\n");
    std::vector<bool> states;
    EXPECT_EQ(reader.processStream(dat,[&](const auto& row){states.push_back(row.digital_values[0]);}),3u);
    ASSERT_EQ(states.size(),3u); EXPECT_TRUE(states[0]); EXPECT_TRUE(states[1]); EXPECT_FALSE(states[2]);
    bool bad=false, nonboolean=false;
    for (const auto& w:reader.getWarnings()) {
        if(w.code=="dat.bad_row") {bad=true; EXPECT_EQ(w.count,3u);}
        if(w.code=="dat.digital_state") nonboolean=true;
    }
    EXPECT_TRUE(bad); EXPECT_TRUE(nonboolean);
}
} // namespace

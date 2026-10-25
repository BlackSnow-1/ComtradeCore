#include <gtest/gtest.h>
#include "../bindings/java/java_api.hpp"
TEST(JavaFacade, LongTimestampsMissingSamplesAndBounds) {
    ComtradeNativeRecord record;
    record.setStationAndDevice("S","D",2013);
    record.addAnalogChannel(1,"A","A","A",1,0);
    record.addSample(4294967296LL,{std::numeric_limits<double>::quiet_NaN()},{});
    EXPECT_EQ(record.timestampAt(0),4294967296LL);
    EXPECT_TRUE(std::isnan(record.analogValueAt(0,0)));
    EXPECT_EQ(record.timeOffsetNanosecondsAt(0),4294967296000LL);
    EXPECT_THROW(record.addSample(-1,{1},{}),std::invalid_argument);
    EXPECT_THROW(record.analogValueAt(0,1),std::out_of_range);
}

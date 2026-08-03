/**
* @file types.hpp
 * @brief COMTRADE data structures and enumerations.
 * @author 王广博
 */
#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <cstdint>

namespace comtrade {

    // 定义纳秒级别的 TimePoint 别名
    using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;


    enum class StandardVersion {
        V1991 = 1991,
        V1999 = 1999,
        V2013 = 2013,
        UNKNOWN = 0
    };

    enum class DataType {
        ASCII,
        BINARY,
        BINARY32,
        FLOAT32
    };

    struct AnalogChannel {
        int index;
        std::string id;
        std::string phase;
        std::string ccbm;
        std::string uu;
        double a = 1.0;
        double b = 0.0;
        double skew = 0.0;
        int min = 0;
        int max = 0;
        double primary = 1.0;
        double secondary = 1.0;
        std::string ps = "P";
    };

    struct DigitalChannel {
        int index;
        std::string id;
        std::string phase;
        std::string ccbm;
        int normal_state = 0;
    };

    struct CfgData {
        std::string station_name;
        std::string rec_dev_id;
        StandardVersion version = StandardVersion::V1991;

        int total_channels = 0;
        int analog_count = 0;
        int digital_count = 0;

        std::vector<AnalogChannel> analog_channels;
        std::vector<DigitalChannel> digital_channels;

        double line_frequency = 50.0;
        DataType data_type = DataType::ASCII;

        // 使用 chrono::time_point 替代原来的 std::string
        TimePoint start_time{};
        TimePoint trigger_time{};
    };

    struct RecordData {
        std::vector<uint32_t> timestamp;
        std::vector<std::vector<double>> analog_values;  // [channel][sample_index]
        std::vector<std::vector<bool>> digital_values;   // [channel][sample_index]
    };

} // namespace comtrade
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
#include <algorithm>

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

    // 封装类：用于 DataType 的转换，不暴露全局函数
    class DataTypeUtils {
    public:
        /**
         * @brief 将 DataType 枚举转换为字符串
         */
        static std::string ToString(DataType type) {
            switch (type) {
                case DataType::ASCII: return "ASCII";
                case DataType::BINARY: return "BINARY";
                case DataType::BINARY32: return "BINARY32";
                case DataType::FLOAT32: return "FLOAT32";
                default: return "ASCII";
            }
        }

        /**
         * @brief 将字符串转换为 DataType 枚举 (忽略大小写)
         */
        static DataType FromString(const std::string &str) {
            std::string upper_str = str;
            std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(),
                           [](const unsigned char c) { return std::toupper(c); });

            if (upper_str == "ASCII") {
                return DataType::ASCII;
            }
            if (upper_str == "BINARY") {
                return DataType::BINARY;
            }
            if (upper_str == "BINARY32") {
                return DataType::BINARY32;
            }
            if (upper_str == "FLOAT32") {
                return DataType::FLOAT32;
            }
            throw std::invalid_argument("Unknown COMTRADE DataType string: " + str);
        }

        // 禁用构造函数，将其作为一个纯静态工具类
        DataTypeUtils() = delete;

        ~DataTypeUtils() = delete;
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
        std::string station_name; /* 厂站名 */
        std::string rec_dev_id; /* 设备ID */
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
        std::vector<std::vector<double> > analog_values; // [channel][sample_index]
        std::vector<std::vector<bool> > digital_values; // [channel][sample_index]
    };
} // namespace comtrade

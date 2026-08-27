/**
* @file types.hpp
 * @brief COMTRADE data structures and enumerations.
 * @author 王广博
 */
#pragma once

#include <chrono>
#include <cmath>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>

namespace comtrade {
    // CFG 的开始/触发时刻保留纳秒精度；DAT 中的时间戳则是相对开始时刻的偏移。
    using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

    // 版本决定 CFG 尾部是否包含 TIMEMULT 和 2013 时间质量字段。
    enum class StandardVersion {
        V1991 = 1991,
        V1999 = 1999,
        V2013 = 2013,
        UNKNOWN = 0
    };

    // DAT 的物理编码。当前 Record/StreamReader 只解析 ASCII，StreamWriter 可写全部类型。
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


    // 模拟通道换算关系：工程值 = DAT 原始值 * a + b。
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

    // 数字通道在 ASCII 中占一列，在二进制 DAT 中每 16 路打包为一个 16-bit word。
    struct DigitalChannel {
        int index;
        std::string id;
        std::string phase;
        std::string ccbm;
        int normal_state = 0;
    };

    // end_sample 是该采样率段最后一个采样点的序号，而不是该段的样本数量。
    struct SampleRate {
        double samples_per_second = 0.0;
        uint32_t end_sample = 0;
    };

    // 一份 CFG 的内存表示。解析 GB 系编码后，所有文本字段统一保存为 UTF-8。
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

        // DAT 第二列为原始时间标记；实际微秒偏移 = 原始值 * time_multiplier。
        std::vector<SampleRate> sample_rates;
        // 2013 版允许 nrates=0 后继续列出不定数量的采样段；该标志用于保持原始语义并正确回写。
        bool variable_sample_rate = false;
        double time_multiplier = 1.0;

        // IEEE/IEC C37.111-2013 time metadata. These two CFG lines are emitted
        // only for revision 2013 records.
        std::string time_code = "UTC";
        std::string local_code = "+0";
        std::string time_quality_code = "F";
        // 0=no adjustment, 1=added, 2=subtracted, 3=time source cannot handle leap seconds.
        int leap_second = 0;

        // Number of fractional second digits used when serializing CFG time
        // fields. TimePoint itself retains nanosecond precision.
        uint8_t timestamp_fractional_digits = 9;

        // 使用 chrono::time_point 替代原来的 std::string
        TimePoint start_time{};
        TimePoint trigger_time{};
    };

    // 波形按“通道优先”存储，便于对整条通道进行批量计算。
    struct RecordData {
        std::vector<uint32_t> timestamp;
        std::vector<std::vector<double> > analog_values; // [channel][sample_index]
        std::vector<std::vector<bool> > digital_values; // [channel][sample_index]
    };
} // namespace comtrade

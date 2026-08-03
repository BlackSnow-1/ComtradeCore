
#pragma once
#include "types.hpp"
#include <fstream>
#include <string>
#include <functional>

namespace comtrade {

    // 定义一行数据的数据结构 (复用，每次回调覆写以节省内存)
    struct SampleRow {
        uint32_t index;
        uint32_t timestamp_us;
        std::vector<double> analog_values;
        std::vector<bool> digital_values;
    };

    class StreamReader {
    private:
        CfgData cfg_;

    public:
        // 解析 CFG (逻辑与之前相同，这里假设已加载)
        void loadCfg(const std::string& cfg_filepath) {
            // ... 解析 CFG 逻辑 ...
        }

        // 流式读取 DAT，并通过回调函数抛出数据
        // 返回值代表成功处理的行数
        size_t processDatStream(const std::string& dat_filepath,
                                std::function<void(const SampleRow&)> on_row_parsed) {

            std::ifstream dat_file(dat_filepath);
            if (!dat_file.is_open()) return 0;

            std::string line;
            size_t parsed_count = 0;

            // 预分配内存，循环复用，将堆内存分配开销降到 0
            SampleRow row_buffer;
            row_buffer.analog_values.resize(cfg_.analog_count);
            row_buffer.digital_values.resize(cfg_.digital_count);

            while (std::getline(dat_file, line)) {
                // 1. 解析单行字符串 (以 ASCII 为例)
                // ... 使用 stringstream 或 sscanf 解析 line ...

                // 2. 将真实值算好放入 row_buffer
                // row_buffer.analog_values[i] = raw_val * a + b;

                // 3. 触发回调，把数据交给上层业务
                on_row_parsed(row_buffer);

                parsed_count++;
            }

            return parsed_count; // 文件读取完毕，整个过程中内存占用始终只有当前这一行的大小
        }
    };

} // namespace comtrade
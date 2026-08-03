# ComtradeCore
一个轻量、零依赖的现代 C++ 核心库，专为电力系统暂态数据（COMTRADE）的解析与内存构造而设计。

## ✨ 特性

* **Header-Only**：仅由头文件组成，零外部依赖，极易集成。
* **现代 C++**：基于 C++17 标准构建，保证类型安全与高性能。
* **多版本兼容**：向下兼容 IEEE C37.111 的 1991、1999 与 2013 版标准。
* **扩展性强**：数据结构清晰，非常容易与 `nlohmann/json` 等库结合进行二次开发。

## 📂 目录结构

```text
ComtradeCore/
├── CMakeLists.txt
├── README.md
├── include/
│   └── comtrade/
│       ├── comtrade.hpp      # 统一引入头文件
│       ├── types.hpp         # 数据结构定义
│       ├── utils.hpp         # 内部字符串与类型工具
│       └── parser.hpp        # CFG/DAT 核心解析逻辑
├── examples/
│   └── read_comtrade.cpp     # 快速上手示例
└── tests/
    └── test_parser.cpp       # 单元测试

这里为你准备了完整的 README 文件、使用示例（`examples/read_comtrade.cpp`）以及基础单元测试（`tests/test_parser.cpp`）。你可以直接将这些文本复制到你的项目中。

### 1. 📖 README.md

```markdown
# ComtradeCore
一个轻量、零依赖的现代 C++ 核心库，专为电力系统暂态数据（COMTRADE）的解析与内存构造而设计[cite: 1]。

## ✨ 特性

* **Header-Only**：仅由头文件组成，零外部依赖，极易集成。
* **现代 C++**：基于 C++17 标准构建，保证类型安全与高性能。
* **多版本兼容**：向下兼容 IEEE C37.111 的 1991、1999 与 2013 版标准。
* **扩展性强**：数据结构清晰，非常容易与 `nlohmann/json` 等库结合进行二次开发。

## 📂 目录结构

```text
ComtradeCore/
├── CMakeLists.txt
├── README.md
├── include/
│   └── comtrade/
│       ├── comtrade.hpp      # 统一引入头文件
│       ├── types.hpp         # 数据结构定义
│       ├── utils.hpp         # 内部字符串与类型工具
│       └── parser.hpp        # CFG/DAT 核心解析逻辑
├── examples/
│   └── read_comtrade.cpp     # 快速上手示例
└── tests/
    └── test_parser.cpp       # 单元测试

```

## 🚀 快速集成 (CMake)

本项目是 Header-only 库，你只需在你的 `CMakeLists.txt` 中引入本库，并链接 `comtrade::ComtradeCore` 即可：

```cmake
add_subdirectory(path/to/ComtradeCore)
target_link_libraries(your_target PRIVATE comtrade::ComtradeCore)

```

## 💡 快速上手

请参考 `examples/read_comtrade.cpp` 以获取完整的读写示例。

这是一个典型的读取示例，展示了如何解析文件并访问转换后的一次值（如电压、电流采样值）。

```cpp
#include <iostream>
#include <iomanip>
#include <comtrade/comtrade.hpp>

int main() {
    comtrade::Parser parser;
    
    // 假设当前目录下有 test.cfg 和 test.dat
    std::string cfg_file = "test.cfg";
    std::string dat_file = "test.dat";

    // 1. 解析 CFG 文件
    if (!parser.parseCfg(cfg_file)) {
        std::cerr << "Failed to open or parse CFG file: " << cfg_file << "\n";
        return -1;
    }

    const auto& cfg = parser.getCfg();
    std::cout << "=== COMTRADE Info ===\n";
    std::cout << "Station Name: " << cfg.station_name << "\n";
    std::cout << "Device ID: " << cfg.rec_dev_id << "\n";
    std::cout << "Total Channels: " << cfg.total_channels << "\n";
    std::cout << "Analog Channels: " << cfg.analog_count << "\n";
    std::cout << "Digital Channels: " << cfg.digital_count << "\n";

    // 打印前几个模拟通道的信息
    for (int i = 0; i < std::min(cfg.analog_count, 3); ++i) {
        const auto& ac = cfg.analog_channels[i];
        std::cout << " - Analog Ch[" << i << "]: " << ac.id 
                  << " (" << ac.phase << ") Units: " << ac.uu << "\n";
    }

    // 2. 解析 DAT 文件
    if (!parser.parseDat(dat_file)) {
        std::cerr << "Failed to open or parse DAT file: " << dat_file << "\n";
        return -1;
    }

    const auto& data = parser.getData();
    std::cout << "\n=== Data Loaded ===\n";
    std::cout << "Total Samples: " << data.timestamp.size() << "\n";

    if (!data.timestamp.empty() && cfg.analog_count > 0) {
        std::cout << "Sample 0 Timestamp: " << data.timestamp[0] << " us\n";
        std::cout << "Sample 0, Analog Ch 0 Real Value: " 
                  << std::fixed << std::setprecision(4) 
                  << data.analog_values[0][0] << " " << cfg.analog_channels[0].uu << "\n";
    }

    return 0;
}

```
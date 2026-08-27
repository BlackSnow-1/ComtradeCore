# ComtradeCore

[![CI](https://github.com/BlackSnow-1/ComtradeCore/actions/workflows/ci.yml/badge.svg)](https://github.com/BlackSnow-1/ComtradeCore/actions/workflows/ci.yml)

ComtradeCore 是一个轻量、Header-only 的 C++17 COMTRADE 库，用于构造、写入和流式读取电力系统暂态记录。
核心库只依赖 C++ 标准库。

## 功能概览

- 使用 `Record` 在内存中构造记录并生成 CFG、DAT 文件。
- 使用 `StreamWriter` 写入 ASCII、BINARY、BINARY32 和 FLOAT32 DAT 数据。
- 使用 `StreamReader` 逐采样读取 ASCII DAT，避免一次性加载完整文件。
- 生成的 CFG 和 ASCII DAT 在所有平台统一使用 COMTRADE CRLF 行结束符。
- 支持 IEEE C37.111 的 1991、1999 和 2013 版本标识。
- 提供可安装的 CMake package，安装后可通过 `find_package()` 使用。

> 当前 `StreamReader` 和 `Record::parseDat()` 只支持读取 ASCII DAT；BINARY、BINARY32、FLOAT32
> 的流式读取尚未实现。

IEEE/IEC C37.111-2013 CFG 支持采样率段、时间倍率、小数秒精度、时间码、本地时区偏移、
时间质量码和闰秒指示。下一版 PC37.111 仍在制定中，尚无可据以实现的正式发布版本。

## 环境要求

- 支持 C++17 的编译器：GCC、Clang 或 MSVC。
- CMake 3.14 或更高版本。
- GoogleTest 仅在构建单元测试时需要；核心库本身不依赖 GoogleTest。

## 安装与集成

### 方法一：作为 CMake 子项目使用

将 ComtradeCore 放入项目目录，例如：

```text
your-project/
├── CMakeLists.txt
├── src/
└── third_party/
    └── ComtradeCore/
```

在项目的 `CMakeLists.txt` 中添加：

```cmake
add_subdirectory(third_party/ComtradeCore)

add_executable(your_app src/main.cpp)
target_link_libraries(your_app PRIVATE comtrade::ComtradeCore)
```

ComtradeCore 作为子项目使用时，测试和示例默认关闭，不会为下游项目自动下载 GoogleTest。

### 方法二：安装后使用 `find_package`

先配置并安装 ComtradeCore。单配置生成器可执行：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMTRADE_BUILD_TESTS=OFF \
  -DCOMTRADE_BUILD_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX=/path/to/comtrade-install

cmake --build build
cmake --install build
```

Visual Studio、CLion 多配置工具链可以指定配置：

```bash
cmake --build build --config Release
cmake --install build --config Release
```

Windows 安装前缀示例：

```text
-DCMAKE_INSTALL_PREFIX=C:/Libraries/ComtradeCore
```

安装内容包括：

```text
<prefix>/
├── include/comtrade/
├── lib/cmake/ComtradeCore/
└── share/doc/ComtradeCore/
```

然后在使用方项目中：

```cmake
find_package(ComtradeCore CONFIG REQUIRED)

add_executable(your_app src/main.cpp)
target_link_libraries(your_app PRIVATE comtrade::ComtradeCore)
```

如果安装目录不是系统默认搜索路径，在配置使用方项目时传入：

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/path/to/comtrade-install
```

在 CLion 中可将同一参数填写到 **Settings | Build, Execution, Deployment | CMake | CMake options**。

### 方法三：直接使用头文件

也可以将仓库中的 `include/comtrade` 复制到项目的头文件搜索路径，然后启用 C++17：

```cmake
target_compile_features(your_app PRIVATE cxx_std_17)
target_include_directories(your_app PRIVATE /path/to/ComtradeCore/include)
```

## 生成 COMTRADE 文件

下面的示例生成一组 ASCII 格式的 `record.cfg` 和 `record.dat`：

```cpp
#include <comtrade/record.hpp>

#include <iostream>

int main() {
    comtrade::Record record;
    record.setStationAndDevice(
        "GRID_01",
        "RELAY_01",
        comtrade::StandardVersion::V1999);

    comtrade::AnalogChannel voltage;
    voltage.index = 1;
    voltage.id = "VA";
    voltage.phase = "A";
    voltage.uu = "V";
    voltage.a = 0.1;
    voltage.b = -5.0;
    record.addAnalogChannel(voltage);

    comtrade::DigitalChannel trip;
    trip.index = 1;
    trip.id = "TRIP";
    record.addDigitalChannel(trip);

    record.addSample(0, {220.0}, {false});
    record.addSample(250, {221.2}, {true});
    record.getMutableCfg().data_type = comtrade::DataType::ASCII;

    if (!record.saveCfg("record.cfg") || !record.saveDat("record.dat")) {
        std::cerr << "生成 COMTRADE 文件失败\n";
        return 1;
    }
}
```

COMTRADE 记录由同名的配置文件和数据文件组成：

```text
record.cfg
record.dat
```

## 流式读取

`StreamReader` 在构造时必须接收 CFG 路径。CFG 不存在或解析失败时，构造函数会抛出
`std::runtime_error`。

```cpp
#include <comtrade/stream_reader.hpp>

#include <iostream>
#include <stdexcept>

int main() {
    try {
        comtrade::StreamReader reader("record.cfg");

        const auto& cfg = reader.getCfg();
        std::cout << "站名: " << cfg.station_name << '\n';
        std::cout << "模拟通道数: " << cfg.analog_count << '\n';
        std::cout << "数字通道数: " << cfg.digital_count << '\n';

        const std::size_t processed = reader.processDatStream(
            "record.dat",
            [](const comtrade::SampleRow& row) {
                std::cout << "序号: " << row.index
                          << ", 时间戳: " << row.timestamp_us << " us\n";

                for (std::size_t i = 0; i < row.analog_values.size(); ++i) {
                    std::cout << "  模拟量[" << i << "]: "
                              << row.analog_values[i] << '\n';
                }

                for (std::size_t i = 0; i < row.digital_values.size(); ++i) {
                    std::cout << "  数字量[" << i << "]: "
                              << (row.digital_values[i] ? "ON" : "OFF") << '\n';
                }
            });

        std::cout << "共处理 " << processed << " 个采样点\n";
    } catch (const std::runtime_error& error) {
        std::cerr << "加载 COMTRADE 配置失败: " << error.what() << '\n';
        return 1;
    }
}
```

Reader 会把 CFG 中的模拟通道系数应用到 DAT 原始值：

```text
实际值 = 原始值 × a + b
```

### 回调对象的生命周期

为了保持稳定的内存占用，`StreamReader` 会复用同一个 `SampleRow` 缓冲区。不要在回调结束后继续
持有 `row` 的引用、地址，或者它内部容器的引用。

如果需要长期保存采样点，应在回调中复制：

```cpp
std::vector<comtrade::SampleRow> rows;

reader.processDatStream("record.dat", [&](const comtrade::SampleRow& row) {
    rows.push_back(row);
});
```

如果只需要实时统计或转发数据，直接在回调中处理可以保持近似恒定的内存占用。

## 构建并运行测试

```bash
cmake -S . -B build \
  -DCOMTRADE_BUILD_TESTS=ON \
  -DCOMTRADE_BUILD_EXAMPLES=OFF

cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

流式端到端测试会在临时目录生成 CFG、DAT 文件，完成读取验证后自动清理。

在 CLion 中重新加载 CMake 后，可以直接运行 `comtrade_unit_tests` 目标，或只运行：

```text
StreamEngineTest.GeneratesComtradeFilesAndStreamsEverySample
```

### GitHub Actions 自动测试

`.github/workflows/ci.yml` 会在推送到 `main`、创建或更新 Pull Request，以及手动触发时运行。
自动化流程覆盖 Linux GCC、Linux Clang 和 Windows MSVC，并执行：

1. 配置和编译库、示例及 GoogleTest 测试。
2. 通过 CTest 运行全部单元测试。
3. 安装 ComtradeCore 到临时 staging 目录。
4. 使用独立消费者项目验证 `find_package(ComtradeCore CONFIG REQUIRED)`。
5. 上传保留 7 天的安装包构件。

真实 COMTRADE 文件的准确性测试和流式读取基准方法见
[`tests/README.md`](tests/README.md)。常规 CI 会选择性下载一组 Git LFS 测试文件并运行准确性测试；
`.github/workflows/benchmark.yml` 每周或手动运行 Release/GCC 基准，并保存性能结果 30 天。

## 目录结构

```text
ComtradeCore/
├── cmake/                         # CMake package 配置模板
├── examples/                      # 示例程序
├── include/comtrade/
│   ├── comtrade.hpp               # Record 统一入口
│   ├── record.hpp                 # 内存记录的构造、读写
│   ├── stream_reader.hpp          # 流式读取
│   ├── stream_writer.hpp          # 流式写入
│   ├── types.hpp                  # 数据类型和通道定义
│   └── utils.hpp                  # 内部工具
├── tests/                         # GoogleTest 单元测试
├── CMakeLists.txt
└── README.md
```

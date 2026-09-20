# C++ AI 录波分析

AI 功能是纯 C++17、**header-only** 的可选模块，实现代码单独放在自己的子目录
`include/comtrade/ai/`，与核心 COMTRADE 读写实现（`include/comtrade/*.hpp`）物理分开，只有
单一入口 `include/comtrade/ai.hpp` 留在核心头文件旁边、方便 `#include`。没有 Qt、JVM 或 JNI
运行时依赖，也没有一行需要单独编译的 `.cpp` 实现代码（`analysis/cpp/src/main.cpp` 是使用这些头文件
的 CLI 程序，而不是模块本身）。核心 `ComtradeCore` 仍为无 AI 依赖的 header-only 库：默认既不构建
AI，也不安装它（见下方 `COMTRADE_BUILD_AI`/`COMTRADE_INSTALL_AI`），AI 头文件不 `#include` 就不会
给核心库引入任何额外依赖。

## 模块划分

| 头文件 | 内容 | 额外依赖 |
| --- | --- | --- |
| [`ai.hpp`](../include/comtrade/ai.hpp) | 单一入口，`#include` 下面 `ai/` 目录里的全部头文件 | — |
| [`ai/config.hpp`](../include/comtrade/ai/config.hpp) | `Config`：字段校验、`fromJson`/`fromFile`、endpoint 解析 | nlohmann/json |
| [`ai/statistics.hpp`](../include/comtrade/ai/statistics.hpp) | `Statistics`：缩放平方和、含直流的样本加权 RMS | 无（纯标准库） |
| [`ai/evidence.hpp`](../include/comtrade/ai/evidence.hpp) | `summarize()`：本地读取 CFG/DAT，生成证据 JSON | OpenSSL（SHA-256） |
| [`ai/client.hpp`](../include/comtrade/ai/client.hpp) | `makeRequest`/`parseResponse`/`analyze()`：HTTPS 调用模型 | cpp-httplib、OpenSSL |
| [`ai/pdf.hpp`](../include/comtrade/ai/pdf.hpp) | `exportPdf`/`writeNewFile()`：中文 PDF 报告 | libharu |
| [`ai/detail.hpp`](../include/comtrade/ai/detail.hpp) | 内部小工具（`require`/`isClean`），不属于公开 API | 无 |

按需 `#include` 对应子头文件（例如只 `#include <comtrade/ai/evidence.hpp>`）即可只引入需要的依赖
（比如只想用 `summarize()` 而不需要 PDF 时，无需链接 libharu）；`#include <comtrade/ai.hpp>` 则
一次性引入全部功能，与旧版本保持兼容。所有函数都是 `inline` 定义在头文件中，可以放心在多个翻译
单元里 `#include`。

这个模块也有对应的 Java 接口 `comtrade.ComtradeAI`（`bindings/java/ai_java_api.hpp` +
`bindings/java/src/main/java/comtrade/ComtradeAI.java`），只在 `BUILD_JAVA_BINDINGS` 和
`COMTRADE_BUILD_AI` 同时打开时随 `comtrade-core-java.jar` 一起构建，用法见
[README.md](../README.md) 中的“Java 中的 AI 分析”一节。

## 构建

需要 C++17 编译器、CMake 3.18+、Git、OpenSSL 3.x 开发包。

CMake 通过 `FetchContent` 固定到具体提交下载三个依赖，测试使用 GoogleTest：

| 依赖 | 版本 / 提交 | 用途 | 项目主页 |
| --- | --- | --- | --- |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.12.0 (`55f93686c0…`) | 配置、证据、请求/响应 JSON | https://github.com/nlohmann/json |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | v0.28.0 (`adf58bf474…`) | 纯 C++ HTTPS 客户端（header-only） | https://github.com/yhirose/cpp-httplib |
| [libharu](https://github.com/libharu/libharu) | v2.4.4 (`0c598becaa…`) | 纯 C PDF 生成，无 Qt 依赖 | https://github.com/libharu/libharu |
| [OpenSSL](https://www.openssl.org/) | 3.0 及以上 | TLS、证书校验、SHA-256 | https://www.openssl.org/ |
| [GoogleTest](https://github.com/google/googletest) | v1.14.0 | 单元与集成测试（仅测试用） | https://github.com/google/googletest |

cpp-httplib 使用其官方 CMakeLists.txt 构建，直接得到 `httplib::httplib` 这个 INTERFACE target：
它自带头文件搜索路径、按需开启的 OpenSSL 支持（`CPPHTTPLIB_OPENSSL_SUPPORT`）以及自身的安装规则，
`ComtradeAI` 只需要 `target_link_libraries(... httplib::httplib)`，不需要手工指定其源码目录或宏定义。
libharu 只负责 PDF，关闭 PNG/Zlib 依赖，编译为静态库以避免额外运行时；它没有自带的 CMake 包配置，
因此其 `hpdf` 目标随 `ComtradeAI` 一并纳入本项目自己的 `ComtradeAITargets` 导出集合。

```sh
# Ubuntu
sudo apt-get install libssl-dev fonts-wqy-microhei poppler-utils
cmake -S . -B build-ai -DCMAKE_BUILD_TYPE=Release -DCOMTRADE_BUILD_AI=ON -DCOMTRADE_BUILD_TESTS=ON
cmake --build build-ai --config Release --parallel 2
ctest --test-dir build-ai -C Release --output-on-failure
cmake --install build-ai --config Release --prefix staging
```

Windows 可用 MSVC + vcpkg 的 `openssl:x64-windows`，通过 `OPENSSL_ROOT_DIR` 指定安装目录；
MinGW 应使用兼容的 OpenSSL 库。macOS 使用 `brew install openssl@3` 并设置 `OPENSSL_ROOT_DIR`。
离线构建可设置 `FETCHCONTENT_SOURCE_DIR_AI_HTTP`、`FETCHCONTENT_SOURCE_DIR_AI_JSON`、
`FETCHCONTENT_SOURCE_DIR_AI_PDF`、`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` 指向对应固定版本源码。

### 是否构建、是否安装是两个独立开关

| CMake 选项 | 默认值 | 作用 |
| --- | --- | --- |
| `COMTRADE_BUILD_AI` | `OFF` | 是否配置 `analysis/cpp`：获取三个依赖、定义 `comtrade::ComtradeAI`、`comtrade-ai` CLI 和（若同时开启 `COMTRADE_BUILD_TESTS`）AI 单测。 |
| `COMTRADE_INSTALL_AI` | `ON` | 只在 `COMTRADE_BUILD_AI=ON` 时才有意义：是否把 `include/comtrade/ai.hpp`、`include/comtrade/ai/`、`comtrade-ai` CLI、`ComtradeAITargets` 导出和第三方 license 一并纳入 `cmake --install`。 |

关掉 `COMTRADE_INSTALL_AI` 仍然可以在本地构建、跑单测（例如 CI 只想验证 AI 代码正确性，但发布包
不想携带 AI 及其依赖头文件的场景）；关闭后 `cmake --install` 完全不会在安装目录里留下任何 AI 相关
文件（`include/comtrade/` 下也不会出现空的 `ai/` 目录），下游 `find_package(ComtradeCore COMPONENTS
AI)` 会正确报告 AI 组件不可用。

## 命令行

复制 `config/ai-config.example.json` 为被 Git 忽略的 `ai-config.json`，填写 endpoint、model 和字体路径。
Windows 中文字体示例：`C:/Windows/Fonts/simhei.ttf` 或 `C:/Windows/Fonts/simsun.ttc`。

```sh
# 仅本地汇总，不调用模型
comtrade-ai summarize ai-config.json sample.cfg sample.dat evidence.json
# 调用配置的模型，再生成 PDF
comtrade-ai analyze ai-config.json sample.cfg sample.dat report.pdf
```

输出文件不能已存在，父目录必须存在。`summarize` 不要求有效的 endpoint/token。
`analyze` 会上传证据；默认隐藏站点、设备和通道名称，但时间、相别、单位及波形统计仍会发送。
配置、文件路径统一使用 UTF-8；原核心解析器在 Windows 的非系统代码页路径支持受现有窄字符文件接口限制。

凭据优先使用 `tokenEnv` 指定的环境变量，空值回退到 `token`。日志和 PDF 不包含凭据或服务器错误正文。
`headers` 支持供应商附加头；Authorization、Host、Content-Type 等保留头禁止覆盖。
`requestParameters` 允许供应商参数；model/messages/stream/tools 等协议字段禁止覆盖。
未知配置字段会报错，避免拼写错误静默生效。

仅支持非流式 Chat Completions 协议。HTTPS 验证证书链及主机名，`caBundle` 可配置 PEM 根证书。
默认使用 OpenSSL/平台可用的信任根；私有 CA 必须显式配置。HTTP 仅允许显式启用的 localhost/127.0.0.1/[::1]。
不跟随重定向，无应用层重试；请求按最终 UTF-8 JSON 字节数限长，响应按接收字节数限长。
连接、读写和整次 HTTP 调用配置超时；同步系统 DNS 解析的耗时可能超出库的请求超时。
拒绝模型截断、拒答、工具调用、空文本或非法 JSON。

## 配置文件字段参考

`config/ai-config.example.json` 中的每个字段都对应 `comtrade::ai::Config` 的一个成员；
`Config::fromJson` 会拒绝任何未列出的字段名，因此这里不能用 JSON 注释代替这张表。

| 字段 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `endpoint` | string | 空 | Chat Completions 兼容的 HTTPS（或显式允许的 loopback HTTP）URL；只在 `analyze` 时校验。 |
| `model` | string | 空 | 请求体里的 `model` 字段；只在 `analyze` 时要求非空。 |
| `token` | string | 空 | Bearer token 的兜底值，`tokenEnv` 指定的环境变量非空时优先使用环境变量。 |
| `tokenEnv` | string | `COMTRADE_AI_TOKEN` | 读取 token 的环境变量名；置空则始终使用 `token`。 |
| `pdfFontPath` | string | 空 | 内嵌进 PDF 的 TrueType/TrueType Collection 字体路径，只在 `exportPdf` 时需要。 |
| `caBundle` | string | 空 | 自定义 PEM 根证书路径；留空则使用系统/OpenSSL 默认信任链。 |
| `headers` | object | `{}` | 附加请求头（如供应商专用鉴权头）；Authorization/Host/Content-Type 等保留头会被拒绝。 |
| `requestParameters` | object | `{}` | 透传给模型的额外请求参数（如 `temperature`）；`model`/`messages`/`stream`/`tools` 等协议字段会被拒绝。 |
| `allowInsecureLocalhost` | bool | `false` | 仅当为 `true` 时才允许 `http://` 访问 `localhost`/`127.0.0.1`/`[::1]`，用于本地联调。 |
| `includeIdentifiers` | bool | `false` | 是否在证据 JSON 中包含站名、设备名和通道原始 `id`。 |
| `waveformMode` | `"summary"` \| `"full"` | `"summary"` | `full` 会把每个有效采样点发送给模型（受下方字节/数量上限约束）。 |
| `timeoutMs` | int > 0 | `60000` | 连接、读写和整次 HTTP 调用统一使用的超时（毫秒）。 |
| `maxRequestBytes` | int > 0 | `4000000` | 最终 UTF-8 JSON 请求体的字节上限，超限直接拒绝、不发送。 |
| `maxResponseBytes` | int > 0 | `1000000` | 模型响应按接收字节数计的上限，超限中止连接。 |
| `maxChannels` | int > 0 | `512` | 模拟量 + 数字量通道总数上限。 |
| `maxWindows` | int ≥ 2 | `256` | 摘要窗口数量上限，超出后相邻窗口自动合并、保留统计极值。 |
| `windowMs` | int > 0 | `20` | 摘要窗口的初始宽度（毫秒）。 |
| `maxDigitalEvents` | int ≥ 0 | `1000` | 详细列出的数字量变位事件条数上限，超出部分只计数不列出。 |
| `maxWaveformSamples` | int > 0 | `100000` | `waveformMode: "full"` 时允许发送的采样点数上限。 |

## 在安装后的项目里使用这份配置

`Config::fromFile(path)`（[`ai/config.hpp`](../include/comtrade/ai/config.hpp)）封装了"打开文件 →
解析 JSON → `Config::fromJson`"这几步，替代手写 `ifstream` + `nlohmann::json::parse`：

```cpp
#include <comtrade/ai.hpp>

auto config = comtrade::ai::Config::fromFile("ai-config.json");
```

`find_package(ComtradeCore CONFIG REQUIRED COMPONENTS AI)` 成功后，会额外设置
`ComtradeCore_AI_CONFIG_EXAMPLE` 变量，指向已安装的示例配置，方便在使用方项目里直接复制一份初始配置：

```cmake
find_package(ComtradeCore 1.2 CONFIG REQUIRED COMPONENTS AI)
configure_file(${ComtradeCore_AI_CONFIG_EXAMPLE} ${CMAKE_CURRENT_BINARY_DIR}/ai-config.json COPYONLY)
```

## C++ API 与安装包

```cmake
find_package(ComtradeCore 1.2 CONFIG REQUIRED COMPONENTS AI)
target_link_libraries(my_app PRIVATE comtrade::ComtradeAI)
```

`#include <comtrade/ai.hpp>` 提供 `Config::fromJson`/`fromFile`、`summarize`、`makeRequest`、
`parseResponse`、`analyze`、`exportPdf`、`writeNewFile`；也可以按上表只引入需要的子头文件。
测量证据使用 `nlohmann::json`；`Statistics`（`ai/statistics.hpp`）只依赖标准库，可以脱离其余 AI
头文件单独使用或测试。`comtrade::ComtradeAI` 是一个 INTERFACE target：它不编译任何代码，只是把
上面这些头文件需要的 include 路径、编译宏和链接库（nlohmann/json、cpp-httplib、OpenSSL、libharu）
一次性声明好；安装包里的静态产物只有 `hpdf` 这一个真正编译的第三方 PDF 库，以及 `comtrade-ai`
命令行工具。安装内容包括这个 CLI、JSON/PDF 依赖的头文件及 CMake 配置；使用端仍需匹配编译器和
OpenSSL 3.x。发布的 Windows AI 包附带构建环境的 TLS DLL；Linux/macOS 需要安装 OpenSSL 运行库。
这些包不是跨系统、跨 ABI 的独立可执行分发。字体不随产品打包，使用者需选择有许可的字体。

## 分析定义和边界

- 直接调用 C++ `StreamReader` 读取 ASCII/BINARY/BINARY32/FLOAT32，使用 `raw*a+b` 工程量，不重复乘 CT/PT。
- 对输入计算 SHA-256，分析前后哈希不一致则拒绝；调用方应提供不再写入的文件快照。
- 模拟量保留有效计数、最小/最大值及时间、均值、含直流的样本加权 RMS；使用缩放平方和避免平方溢出。
- 按 CFG 触发时间分成触发前和触发起两组。触发不一定等于故障起点；skew 仅作元数据，不做时补偿。
- 默认 20ms 摘要窗口，超出 `maxWindows` 时合并相邻窗口，保留统计极值；窗口按固定时长切分，不随
  `sample_rates` 的采样段边界对齐，也不是电网周期，后期分辨率可能更低。
- 多段不同采样率的录波（IEEE C37.111-2013 允许 `nrates=0` 后跟任意数量采样段）在流式层面已经
  可以逐点区分：`StreamReader::processDatStream` 回调到的每个 `SampleRow` 都带有 `segment_index`
  和 `segment_sample_rate`（见 [`stream_reader.hpp`](../include/comtrade/stream_reader.hpp)），
  按物理采样序号在 `cfg().sample_rates` 中单调前移定位，整份文件的定位总代价是 O(采样段数)。
  `summarize()` 目前仍按固定 `windowMs` 时长摘要，如果需要按采样段边界（而不是固定时长）分别统计，
  可以在回调里读取这两个字段自行分段，不需要重新做二分查找。
- 数字量保留初末状态、总变位次数及前 `maxDigitalEvents` 个事件，明确报告省略数。
- 非递增时间、采样序号断续、非有限值、CFG/物理行数不符都显式报告。残缺二进制行直接拒绝。
- summary 内存随通道数、窗口数、事件上限增长；完整模式按 `maxWaveformSamples` 和字节预算限制，超限失败而非截断。
- `waveformMode: full` 额外发送每个有效采样，列顺序为 sampleIndex/rawTimestamp/timeOffsetNs/模拟量/数字量。
  完整模式拒绝物理行数与解析数不符、CFG 数量不符、非有限模拟量；不自动抽样或分批请求。
- 时间纳秒和原始时间戳以十进制字符串保存，避免接收端 JSON 浮点整数精度损失。
- 原解析器对部分字段容错，因此行数一致不构成原始数据严格符合标准或真实性的证明。
- 尚未实现相量、频率、谐波、序分量、保护定值和故障测距；模型结果只能辅助人工研判。

PDF 用嵌入式 TrueType 字体渲染 UTF-8 中文，支持 TTF 和 TTC 首个字体，自动按字宽换行和分页。
当前支持 Unicode BMP；缺失字形、emoji/扩展平面字符会明确报错，不静默输出方框。
模型文字按纯文本处理。报告区分本地事实、未验证模型解释、证据附录；full 模式只在 PDF 中省略 rows，
不影响发给模型的完整波形。输出在内存中完成后独占创建，失败清理新文件，不覆盖已有文件。

## 测试与 CI/CD

`tests/test_ai.cpp` 是独立的 GoogleTest 可执行文件 `comtrade_ai_tests`，只在 `COMTRADE_BUILD_AI`
和 `COMTRADE_BUILD_TESTS` 都打开时构建（见 [`tests/CMakeLists.txt`](../tests/CMakeLists.txt)），
和只依赖 `comtrade::ComtradeCore` 的核心单测 `comtrade_unit_tests` 是两个可执行文件，互不强制依赖。
覆盖缩放 RMS、极值时间、窗口合并、触发分区、四种 DAT、限额、质量标记、响应校验、配置约束。
集成测试使用本地 HTTP 服务验证真实请求、超时、拒绝重定向、响应上限和 CFG/DAT 到中文 PDF 的完整流程。
Linux 额外用 [`tests/verify_pdf.py`](../tests/verify_pdf.py) 借助 Poppler 验证中文文本提取、多页
报告和实际渲染。
CI 为 Linux GCC/Clang、Windows MSVC、macOS Clang 构建 AI 并执行安装后 CMake 消费者测试；
原有无 AI 核心矩阵继续运行。Release 复用全部 CI，所有作业通过后才打标签并发布已验证的构建附件。
真实付费模型不参与自动测试，无需真实 token。

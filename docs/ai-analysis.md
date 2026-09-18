# C++ AI 录波分析

AI 功能已从 Java 重写为独立的 C++17 模块 `analysis/cpp`。没有 Qt、JVM 或 JNI 运行时依赖。
原有 Java 解析绑定不受影响。核心 `ComtradeCore` 仍为无 AI 依赖的 header-only 库；默认不构建 AI。

## 构建

需要 C++17 编译器、CMake 3.18+、Git、OpenSSL 3.x 开发包。
CMake 固定到具体提交下载 cpp-httplib 0.28.0、nlohmann/json 3.12.0、libharu 2.4.4；测试使用 GoogleTest。
libharu 只负责 PDF，关闭 PNG/Zlib 依赖。HTTP、分析逻辑、CLI 和测试均使用 C++。

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

## C++ API 与安装包

```cmake
find_package(ComtradeCore 1.2 CONFIG REQUIRED COMPONENTS AI)
target_link_libraries(my_app PRIVATE comtrade::ComtradeAI)
```

`<comtrade/ai.hpp>` 提供 `Config::fromJson`、`summarize`、`makeRequest`、`analyze`、`exportPdf`。
测量证据使用 `nlohmann::json`；`Statistics` 位于 `<comtrade/ai_statistics.hpp>`，只依赖标准库。
安装包包括静态 AI/PDF 库、CLI、JSON 头文件及 CMake 配置；使用端仍需匹配编译器和 OpenSSL 3.x。
发布的 Windows AI 包附带构建环境的 TLS DLL；Linux/macOS 需要安装 OpenSSL 运行库。
这些包不是跨系统、跨 ABI 的独立可执行分发。字体不随产品打包，使用者需选择有许可的字体。

## 分析定义和边界

- 直接调用 C++ `StreamReader` 读取 ASCII/BINARY/BINARY32/FLOAT32，使用 `raw*a+b` 工程量，不重复乘 CT/PT。
- 对输入计算 SHA-256，分析前后哈希不一致则拒绝；调用方应提供不再写入的文件快照。
- 模拟量保留有效计数、最小/最大值及时间、均值、含直流的样本加权 RMS；使用缩放平方和避免平方溢出。
- 按 CFG 触发时间分成触发前和触发起两组。触发不一定等于故障起点；skew 仅作元数据，不做时补偿。
- 默认 20ms 摘要窗口，超出 `maxWindows` 时合并相邻窗口，保留统计极值；窗口不是电网周期，后期分辨率可能更低。
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

C++ 单测覆盖缩放 RMS、极值时间、窗口合并、触发分区、四种 DAT、限额、质量标记、响应校验、配置约束。
集成测试使用本地 HTTP 服务验证真实请求、超时、拒绝重定向、响应上限和 CFG/DAT 到中文 PDF 的完整流程。
Linux 额外用 Poppler 验证中文文本提取、多页报告和实际渲染。
CI 为 Linux GCC/Clang、Windows MSVC、macOS Clang 构建 AI 并执行安装后 CMake 消费者测试；
原有无 AI 核心矩阵继续运行。Release 复用全部 CI，所有作业通过后才打标签并发布已验证的构建附件。
真实付费模型不参与自动测试，无需真实 token。

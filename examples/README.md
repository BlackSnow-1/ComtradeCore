# 可运行使用示例

这些示例使用公开 API，演示正常流程以及调用方应如何处理警告和边界情况。
新示例默认自行生成小型 CFG/DAT/CFF，不依赖 Git LFS、厂家文件、网络或真实模型。

## C++ 核心示例

| 源文件 / 目标 | 场景 | 主要输出 |
| --- | --- | --- |
| `record_formats.cpp` / `example_record_formats` | 四种 DAT 编码的创建、读取、工程量编辑、保存 | 每种编码原始和编辑后的 CFG/DAT |
| `stream_processing.cpp` / `example_stream_processing` | 逐点写入、逐点读取 20,000 个采样，计算均值及数字量状态计数 | 二进制 CFG/DAT、前 3 点及汇总 |
| `cff_roundtrip.cpp` / `example_cff_roundtrip` | 四种编码的 CFF、HDR/INF 修改、125 ns 时间偏移、流式读取 | 原始及编辑后的 CFF |
| `time_axis.cpp` / `example_time_axis` | 多采样率、空/零时间戳、三种时间轴策略、UTC 换算、未知时区警告 | 人工构造的 CFG/DAT、各策略结果 |
| `missing_and_warnings.cpp` / `example_missing_and_warnings` | NaN 与四种缺失标记往返、坏行恢复、采样序号及数字量警告 | 四种 DAT 与损坏行样例 |
| `vendor_tail.cpp` / `example_vendor_tail` | 四种编码的厂家尾部隔离、NUL/非 UTF-8 字节、有限预览、分块完整导出、显式复制、错误 CFG 样本数 | 标准 DAT、带尾部副本、原始尾部 `.bin` |
| `inspect_record.cpp` / `example_inspect_record` | 直接检查实际 CFG/DAT 或 CFF，恒定波形内存 | 前 5 点、配置、缺失值数量和警告；不写文件 |

构建核心示例，无需 AI 或 Java 依赖：

```sh
cmake -S . -B build-examples -DCMAKE_BUILD_TYPE=Release -DCOMTRADE_BUILD_EXAMPLES=ON -DCOMTRADE_BUILD_TESTS=OFF
cmake --build build-examples --config Release --parallel 2
```

从仓库根目录运行，例如 Linux/macOS 或使用 Ninja 的 Windows：

```sh
build-examples/bin/example_record_formats
build-examples/bin/example_stream_processing
build-examples/bin/example_cff_roundtrip
build-examples/bin/example_time_axis
build-examples/bin/example_missing_and_warnings
build-examples/bin/example_vendor_tail
```

Windows 可添加 `.exe`；Visual Studio 等多配置生成器的可执行文件在 `build-examples/bin/Release/`。
上述 6 个新示例均接受一个可选参数：**新的输出目录**。不传参数时，在当前工作目录的
`example-output/<场景>/<时间标识>/` 新建独立目录，终端会打印绝对路径；已存在的指定目录会报错，避免覆盖文件。
可重复无参数运行，输出分别保留以便检查。`example_support.hpp` 只复用演示配置、目录和错误处理，不增加库 API。

```sh
build-examples/bin/example_vendor_tail ./my-new-tail-demo
build-examples/bin/example_inspect_record /path/to/input.cfg /path/to/input.dat
build-examples/bin/example_inspect_record /path/to/input.cff
# 仅在确认 CFG 样本数不准确时，绕过声明的尾部边界：
build-examples/bin/example_inspect_record /path/to/input.cfg /path/to/input.dat --parse-to-end
```

### 如何理解输出

- `Record` 保存的是 `raw*a+b` 工程量；整数 DAT 会按系数量化，FLOAT32 也有浮点精度限制。
- `stream_processing` 使用 TIMEMULT=1：5 秒录波若使用 0.001 微秒刻度，会超出二进制 DAT 的 uint32 时间戳范围。
- `time_axis` 的 Automatic / SampleRates 通常输出 0、1,000,000、1,500,000、2,000,000、2,500,000 ns。
  DatTimestamps 则保留零值，并跳过无法获得时间的空时间戳行，发出警告。
- C++ 流式回调复用行对象，回调后继续使用必须复制；缺失值用 `analog_missing` 或 `std::isnan` 检查。
- 厂家尾部依赖 CFG 结束采样号划界，CFG 数量错误时应核对原文件；`ParseToEnd` 不会自动理解厂家的二进制布局。
- 默认保存标准 DAT 会警告尾部未附带；显式复制尾部不更新厂家校验码、偏移或签名，不能保证厂家软件接受修改后的文件。
- 示例内有必要的返回值/结果检查；真正无法继续时打印 `Error:` 并返回非零，恢复性问题打印 `Warning`。

原有 `record_lifecycle.cpp` 和 `example_high_perf_write.cpp` 继续保留，目标分别为
`example_record_lifecycle` 和 `example_example_high_perf_write`；后者演示按通道批量构造三相波形。
这两个原有示例仍在当前目录写固定文件名，运行时请使用独立的工作目录。

## C++ 可选 AI 示例

`ai/local_evidence.cpp`：在本地分别汇总 CFG/DAT 和 CFF，保存 summary/full 证据 JSON，并生成请求体预览。
不调用模型，不需要 token 或字体。

```sh
cmake -S . -B build-examples-ai -DCMAKE_BUILD_TYPE=Release -DCOMTRADE_BUILD_EXAMPLES=ON -DCOMTRADE_BUILD_AI=ON -DCOMTRADE_BUILD_TESTS=OFF
cmake --build build-examples-ai --config Release --parallel 2
build-examples-ai/bin/example_ai_local_evidence
```

`ai/analyze_report.cpp`：使用实际输入和配置，调用模型，再生成中文 PDF。只有显式提供 `--send` 才会发起请求；
它不参与自动试跑。需要配置模型、HTTPS endpoint、凭据和中文字体，配置参考根目录的 `config/ai-config.example.json`。
输出 PDF 必须不存在。不要把真实 token 写进示例源码。

```sh
build-examples-ai/bin/example_ai_analyze_report ai-config.json /path/to/input.cff report.pdf --send
build-examples-ai/bin/example_ai_analyze_report ai-config.json /path/to/input.cfg /path/to/input.dat report.pdf --send
```

## Java 可运行示例

Java 示例位于 `examples/java/`，包名为 `comtrade.examples`，使用 try-with-resources 管理 JNI 资源。

| 类 | 内容 |
| --- | --- |
| `RecordAndCff` | 四种 DAT/CFF、NaN、内存编辑、HDR/INF、流式回调 |
| `StreamingAndTail` | 流式生成 20,000 点、有限尾部预览、byte[] 原样导出与显式复制 |
| `TimeAndRecovery` | 三种时间轴策略、多采样率、坏行恢复及警告 |
| `AiEvidence` | 本地 CFG/DAT 与 CFF 的 AI 证据 JSON；仅开启 AI 时构建，不访问网络 |

`DemoSupport.java` 提供演示配置与独立输出目录。构建需要 JDK、SWIG 和本机 C++ 编译器：

```sh
cmake -S . -B build-java -DCMAKE_BUILD_TYPE=Release -DBUILD_JAVA_BINDINGS=ON -DCOMTRADE_BUILD_EXAMPLES=ON -DCOMTRADE_BUILD_TESTS=OFF
cmake --build build-java --config Release --target ComtradeJavaExamples --parallel 2
```

Linux/macOS 的运行命令（切换最后的类名可运行其他场景）：

```sh
java -Djava.library.path=build-java/bindings/java -cp "build-java/bindings/java/comtrade-core-java.jar:build-java/bindings/java/examples/classes" comtrade.examples.RecordAndCff
```

Windows PowerShell：

```powershell
java "-Djava.library.path=build-java/bindings/java" -cp "build-java/bindings/java/comtrade-core-java.jar;build-java/bindings/java/examples/classes" comtrade.examples.StreamingAndTail
```

Visual Studio 构建时，将 `java.library.path` 改为 `build-java/bindings/java/Release`。
JAR 与 classes 路径不变。新 JDK 如提示 native-access 警告，可在主类之前加 `--enable-native-access=ALL-UNNAMED`。
Java 示例同样支持一个可选的新输出目录参数。Java 回调数组是独立副本；尾部为 byte[]，不要当作 UTF-8 强制解码。

要运行 `AiEvidence`，在上述 CMake 配置中再加 `-DCOMTRADE_BUILD_AI=ON`，重新构建，然后运行
`comtrade.examples.AiEvidence`。本机还必须能加载 AI 依赖的 OpenSSL 等运行库。

## 自动试跑

启用 `COMTRADE_BUILD_TESTS=ON` 后，6 个自包含 C++ 场景会注册为 `Examples.*` CTest 项。
启用 AI 会增加离线证据示例；启用 Java 会增加对应 Java 场景。实际文件检查工具和真实模型请求示例不自动运行。

```sh
ctest --test-dir build-examples -C Release -L examples --output-on-failure
```

CI 在核心多平台矩阵试跑 C++ 示例，在 Java 作业中运行实际 JVM 示例。所有自动运行的示例均不调用真实模型。

字段异常的恢复规则和警告代码见 [`docs/validation.md`](../docs/validation.md)。
可选 AI 的 OpenSSL 本地优先、GitHub 源码回退与离线配置见
[`docs/ai-analysis.md`](../docs/ai-analysis.md#openssl-本地优先与-github-源码回退)。

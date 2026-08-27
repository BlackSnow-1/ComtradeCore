# COMTRADE 测试数据使用说明

## 测试分层

### 准确性测试

`test_real_files.cpp` 参数化覆盖当前测试数据中的全部 6 组 ASCII 录波：

```text
ComtradeFiles/cometrade/000002.CFG + 000002.DAT                  (GB2312)
ComtradeFiles/cometrade/000003.CFG + 000003.DAT                  (GB2312)
ComtradeFiles/cometrade/000004.CFG + 000004.DAT                  (GB2312)
ComtradeFiles/SIMENS/20191024045947.CFG + 20191024045947.DAT      (GB2312)
ComtradeFiles/Transient/*_ID_1_*_TRIP.cfg + 对应 DAT              (UTF-8)
ComtradeFiles/Transient/*_ID_2_*_TRIP.cfg + 对应 DAT              (UTF-8)
```

`FaultData` 和 `testfile` 中当前没有声明为 ASCII 的 CFG，因此不会交给仅支持 ASCII 的
`StreamReader`。参数化测试验证：

- CFG 能否加载，数据类型是否为 ASCII；
- GB2312 和 UTF-8 CFG 是否都能完成结构及数值解析；
- 通道声明与每一行的数据维度是否一致；
- 所有采样序号是否连续；
- 原始时间戳是否单调，以及按 `TIMEMULT` 换算后的微秒、纳秒偏移和绝对时间是否一致；
- 实际读取行数是否等于 CFG 声明的结束采样号。

其中 Siemens 基线另外包含 12 路模拟量、29 路数字量和 1740 个采样点的精确断言，继续验证：

- 频率和采样率；
- 设备产生的 1 微秒舍入特征；
- 所有模拟通道的累计换算结果；
- 所有数字通道的置位次数；
- 首尾采样的时间戳和代表性通道值。

CFG 的结构字段、数字和分隔符都是 ASCII 子集。解析器会先验证整份 CFG 是否为合法 UTF-8；
若不是，则按 GB18030 解码（兼容 GB2312 和 GBK），将站名、设备名和通道信息统一转换为 UTF-8。
Linux 使用系统 `iconv`，Windows 使用系统代码页 54936；其他平台若无法提供转换能力，则保留原始字节，
仍可继续解析 COMTRADE 数值结构。

固定基线值应由独立工具或人工计算产生，不要直接调用待测解析器生成期望值，否则同一个错误可能同时存在于结果和基线中。

测试数据由 Git LFS 管理。若本地只有指针文件，先执行：

```bash
git lfs pull \
  --include="tests/ComtradeFiles/cometrade/*.CFG,tests/ComtradeFiles/cometrade/*.DAT,tests/ComtradeFiles/SIMENS/20191024045947.CFG,tests/ComtradeFiles/SIMENS/20191024045947.DAT,tests/ComtradeFiles/Transient/*.cfg,tests/ComtradeFiles/Transient/*.dat"
```

运行准确性测试：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMTRADE_BUILD_TESTS=ON \
  -DCOMTRADE_BUILD_EXAMPLES=OFF
cmake --build build --config Release --parallel
ctest --test-dir build -C Release -L accuracy --output-on-failure
```

### 基准测试

`benchmarks/stream_read_benchmark.cpp` 默认对同一真实 DAT 文件重复读取 100 次，报告：

- 总耗时；
- MiB/s；
- samples/s；
- 每个采样点耗时；
- 防止编译器消除数据处理的 checksum。

构建和运行：

```bash
cmake -S . -B build-benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMTRADE_BUILD_TESTS=OFF \
  -DCOMTRADE_BUILD_EXAMPLES=OFF \
  -DCOMTRADE_BUILD_BENCHMARKS=ON
cmake --build build-benchmark --config Release \
  --target comtrade_stream_benchmark --parallel
./build-benchmark/benchmarks/comtrade_stream_benchmark
```

也可以指定其他 ASCII 文件和循环次数：

```bash
./build-benchmark/benchmarks/comtrade_stream_benchmark \
  path/to/record.cfg path/to/record.dat 200
```

基准程序不设置固定性能门槛。共享 CI 主机的性能波动较大，应观察多次运行的趋势；若要设置回归阈值，应在固定硬件、自托管 runner 上建立历史基线。

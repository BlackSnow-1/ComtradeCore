# COMTRADE 测试数据使用说明

## 测试分层

### 准确性测试

`test_real_files.cpp` 使用真实录波文件：

```text
ComtradeFiles/SIMENS/20191024045947.CFG
ComtradeFiles/SIMENS/20191024045947.DAT
```

该基线包含 12 路模拟量、29 路数字量和 1740 个 ASCII 采样点。测试验证：

- CFG 通道数量、数据类型、频率和采样率；
- 所有采样序号是否连续；
- 时间戳是否单调，以及设备产生的 1 微秒舍入特征；
- 所有模拟通道的累计换算结果；
- 所有数字通道的置位次数；
- 首尾采样的时间戳和代表性通道值。

固定基线值应由独立工具或人工计算产生，不要直接调用待测解析器生成期望值，否则同一个错误可能同时存在于结果和基线中。

测试数据由 Git LFS 管理。若本地只有指针文件，先执行：

```bash
git lfs pull \
  --include="tests/ComtradeFiles/SIMENS/20191024045947.CFG,tests/ComtradeFiles/SIMENS/20191024045947.DAT"
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

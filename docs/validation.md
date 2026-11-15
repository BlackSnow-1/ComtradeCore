# 字段校验与容错

## IEEE 格式检查

[格式检查工具](../tools/README.md)支持单独检查 CFG，也可逐行或逐记录交叉检查完整 DAT。C++ 可调用 `comtrade::checkFormat(cfgStream, &datStream)`，传入以二进制模式打开的输入流。省略第二个参数时只检查 CFG。`FormatReport` 返回预期与实际采样数、I/O 状态和诊断列表。

`Record` 和 `StreamReader` 读取 CFG/DAT 时自动检查文件换行、通道编号和列数、采样序列、数值及其他 IEEE 字段。CFF 的内嵌 CFG 和 DAT 也走这些检查。默认向 `std::clog` 输出文件名、首次问题位置、代码和累计次数。C++ 调用方可在读取前设置 `ReadOptions::warning_logger`，将诊断接入应用日志。将其设为 `{}` 可关闭日志输出，并通过 `getWarnings()` 查询诊断。Java 绑定通过本地日志输出警告，仍可用 `warnings()` 查询。

```cpp
comtrade::Record record;
comtrade::ReadOptions opts;
opts.warning_logger = [](const std::string& source, const comtrade::Warning& warning) {
    comtrade::logWarning(source, warning);
};
record.setReadOptions(opts);
if (record.parseCfg("event.cfg")) record.parseDat("event.dat");
```

默认容错读取会在 CFG 的采样边界保留厂商附加字节。要逐记录排查边界后的内容，可用独立工具或指定 `DatTailPolicy::ParseToEnd`。标准 ASCII EOF 标记（0x1A）在读取时识别，1991 和 2013 版写入器在关闭时生成该标记。

## 读取时的修复与告警


核心 CFG/DAT 解析遵循“可以可靠恢复则继续并警告；不能可靠解码则失败或跳过坏行”。
Record 和 StreamReader 共用解析器，CFF 内嵌 CFG 也遵循相同规则。

| 字段或异常 | 行为 | 警告代码 |
| --- | --- | --- |
| 总通道数错误或不是完整整数 | 使用模拟量与数字量通道数之和 | cfg.total_default / cfg.channel_count |
| A/D 通道数缺少后缀或使用小写后缀 | 数字完整、非负且结构可解析时接受 | cfg.channel_suffix |
| A/D 通道数、采样段数损坏或超出范围 | 失败，不猜测后续行的布局 | — |
| 模拟通道 a/b 不是完整有限数字 | 失败，避免错误工程量 | — |
| 通道编号缺失或格式错误 | 使用该类通道的顺序编号 | cfg.optional_default |
| 编号不是顺序编号 | 保留原编号 | cfg.channel_index |
| 可选数字元数据缺失、越界或格式错误 | 保留默认值；整数不接受小数 | cfg.optional_default |
| 频率损坏或非正数 | 使用 50 Hz | cfg.frequency |
| TIMEMULT 损坏或非正数 | 使用 1 | cfg.timemult |
| 闰秒标记损坏或超出 0～3 | 使用 0 | cfg.leap_second |
| 数字通道正常状态不是 0/1 | 保留，并警告 | cfg.digital_state |
| 可疑量程、变比或 P/S 元数据 | 保留，并警告 | cfg.analog_metadata |
| 未知时间质量、本地时间码、不可用时区 | 警告；未知时区按本机时区解释 | cfg.time_quality / cfg.local_code / cfg.timezone_unknown |
| 已知行中的额外 CFG 字段 | 忽略，并警告 | cfg.extra_columns |
| CFG 尾部扩展 | 源文件中保留，规范化回写不保留 | cfg.trailing |
| 无效日期、未知 DAT 编码 | 失败 | — |
| DAT 序号为 0、数字字段带垃圾后缀、数字量非整数 | 跳过坏行 | dat.bad_row |
| ASCII DAT 数字量是其他有符号整数 | 按非零为 true 读取，并警告 | dat.digital_state |
| 缺失模拟量 | 保留采样点，输出 NaN 和缺失标记 | dat.missing_analog |

警告按 code 聚合，包含 message、first_row 和 count；first_row 为 0 表示没有具体行号。
每次读取 DAT 时重置读取警告并保留 CFG 警告。请检查警告，不要以“读取成功”替代数据质量判断。
加载失败仍使用现有接口约定：Record 返回 false，StreamReader 路径构造函数抛出异常。
内部 parseCfgLines 的数值转换异常由文件/CFF 入口捕获。

本次回归测试在 `tests/test_cfg_validation.cpp`，其他缺失值、时间重建、CFF、厂家尾部测试分别在
`tests/test_extensions.cpp` 和 `tests/test_dat_tail.cpp`。运行：

```bash
cmake --build build --config Release --parallel
ctest --test-dir build -C Release -L unit --output-on-failure --no-tests=error
```

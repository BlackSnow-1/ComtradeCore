# COMTRADE 格式检查工具

在 `tools/` 下提供 Windows x86-64 和 Linux x86-64 独立可执行文件。也可从 [GitHub Releases](https://github.com/BlackSnow-1/ComtradeCore/releases) 下载。Linux 发布资产为 tar.gz，解包后保留执行权限。

PowerShell：

```powershell
.\tools\comtrade-check-windows-x86_64.exe --cfg 'D:\录波\event.cfg'
.\tools\comtrade-check-windows-x86_64.exe 'D:\录波\event.cfg' 'D:\录波\event.dat'
$LASTEXITCODE
```

Linux：

```sh
./tools/comtrade-check-linux-x86_64 --cfg ./event.cfg
./tools/comtrade-check-linux-x86_64 ./event.cfg ./event.dat
echo $?
```

只传入 CFG 路径时，工具查找同目录同主文件名的 `.dat` 或 `.DAT`。加 `--cfg` 可仅检查配置。读取操作不会修改文件。DAT 逐行或逐记录扫描。警告输出到 stderr，包含文件、首次问题行号（ASCII）或记录序号（二进制）、诊断代码和出现次数。最终结果输出到 stdout。退出码 `0` 表示检查通过，`1` 表示发现问题，`2` 表示参数或输入读取错误。

| 检查对象 | 主要规则 |
| --- | --- |
| CFG 原始文本 | ASCII 字符、CRLF、CSV 字段数、版本和字段顺序 |
| CFG 通道 | 总数与 A/D 小计、分类型从 1 编号、通道字段长度、量程、比例字段及状态通道正常值 |
| CFG 采样与时间 | 采样段数、采样率与递增的 `endsamp`、日期时间、`timemult`、2013 时间码、时间质量及闰秒 |
| ASCII DAT | 行列数、CRLF、适用版本要求的 EOF 字节、采样号、时间戳、模拟量及状态值、版本对应的缺失值 |
| 二进制 DAT | 按 CFG 计算记录宽度、尾部残缺记录、采样号、时间戳、模拟量范围、FLOAT32 有限值和 2013 状态字高位填充 |
| CFG/DAT 配对 | 主文件名、通道布局、DAT 记录数和 CFG 最终 `endsamp` |

规则依据为 [IEEE C37.111-1991](https://standards.ieee.org/ieee/C37.111/2644/) 第 6.2–6.5 节、[IEEE C37.111-1999](https://standards.ieee.org/ieee/C37.111/2645/) 第 5–6 章，以及 [IEEE C37.111-2013 / IEC 60255-24:2013](https://standards.ieee.org/ieee/C37.111/3795/) 第 7–8 章。工具范围为单对 CFG/DAT 的结构和数据一致性。CFF 内嵌 CFG/DAT 可由项目解析器检查并记录警告。多卷录波、HDR/INF 内容、1991 二进制转换程序和工程语义由使用方另行核对。

从源码构建：

```sh
cmake -S . -B build -DCOMTRADE_BUILD_TOOLS=ON -DCOMTRADE_BUILD_TESTS=OFF -DCOMTRADE_BUILD_EXAMPLES=OFF
cmake --build build --config Release --target comtrade-check
```

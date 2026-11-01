# Java 安装后独立消费者测试

分别验证 `COMTRADE_BUILD_AI=OFF` 和 `ON`。测试程序需要 JDK 11+，仅依赖安装后的
`comtrade-core-java.jar` 编译；运行时 `java.library.path` 仅指定安装目录，不指向生产者构建目录。
测试还断言 JAR 的实际加载位置，以及 AI 类是否与构建选项一致，AI 缺失不能被当作跳过。

覆盖四种 DAT 编码的工程量读写、CFF/HDR、流式纳秒时间、AI CFG/DAT 摘要和 CFF 完整波形、
本机 TCP 模拟模型服务的请求/响应及中文 PDF 导出。HTTP 测试不调用外部模型、不需要密钥；
它不代替 C++ 中已有的 TLS 证书验证测试。

以下为 Linux/macOS 示例，从项目根目录执行。先准备 JDK、SWIG、C++ 编译器；AI 模式另需
可用的依赖库和中文 TTF/TTC 字体（依赖的本地优先/下载规则见 `docs/ai-analysis.md`）。

```bash
cmake -S . -B build-java-core -DBUILD_JAVA_BINDINGS=ON \
  -DCOMTRADE_BUILD_AI=OFF -DCOMTRADE_BUILD_TESTS=OFF -DCOMTRADE_BUILD_EXAMPLES=OFF
cmake --build build-java-core --config Release --target ComtradeCoreJavaJar --parallel 2
cmake --install build-java-core --config Release --component Java --prefix "$PWD/install-java-core"
cmake -S .github/java-install-test -B build-java-core-consumer \
  -DCOMTRADE_JAVA_DIR="$PWD/install-java-core/lib/comtrade/java" -DCOMTRADE_EXPECT_AI=OFF
cmake --build build-java-core-consumer
ctest --test-dir build-java-core-consumer --output-on-failure --no-tests=error

cmake -S . -B build-java-ai -DBUILD_JAVA_BINDINGS=ON \
  -DCOMTRADE_BUILD_AI=ON -DCOMTRADE_BUILD_TESTS=OFF -DCOMTRADE_BUILD_EXAMPLES=OFF
cmake --build build-java-ai --config Release --target ComtradeCoreJavaJar --parallel 2
cmake --install build-java-ai --config Release --component Java --prefix "$PWD/install-java-ai"
cmake -S .github/java-install-test -B build-java-ai-consumer \
  -DCOMTRADE_JAVA_DIR="$PWD/install-java-ai/lib/comtrade/java" -DCOMTRADE_EXPECT_AI=ON \
  -DCOMTRADE_TEST_FONT=/usr/share/fonts/truetype/wqy/wqy-microhei.ttc
cmake --build build-java-ai-consumer
ctest --test-dir build-java-ai-consumer --output-on-failure --no-tests=error
```

若自定义了 `CMAKE_INSTALL_LIBDIR`，相应调整 `COMTRADE_JAVA_DIR`。
`Java` 安装组件只安装 JAR 和 JNI 库，不复制第三方运行库；运行环境仍需匹配的 C++ 运行库，
AI 动态链接时还需匹配的 OpenSSL。Windows 按实际运行库安装配置 PATH，Linux 配置系统库路径或
LD_LIBRARY_PATH，macOS 使用对应运行库路径；不要把生产者的 JNI 构建目录加入搜索路径来替代安装。
Windows 可以使用 Ninja 运行同一个消费者项目；CMake 会处理 classpath 分隔符。

CI 的 Java 作业对 OFF/ON 两种模式分别构建、安装、编译独立消费者并运行，上传 JUnit 和测试日志。

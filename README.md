# ComtradeCore

[![CI](https://github.com/BlackSnow-1/ComtradeCore/actions/workflows/ci.yml/badge.svg)](https://github.com/BlackSnow-1/ComtradeCore/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](#环境要求)
[![CMake 3.14+](https://img.shields.io/badge/CMake-3.14%2B-064F8C?logo=cmake&logoColor=white)](#安装与集成)
[![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

[![Header-only](https://img.shields.io/badge/Architecture-Header--only-2ea44f)](#安装与集成)
[![COMTRADE](https://img.shields.io/badge/COMTRADE-1991%20%7C%201999%20%7C%202013-6f42c1)](#功能概览)
[![Platforms](https://img.shields.io/badge/Platforms-Linux%20%7C%20Windows%20%7C%20macOS-informational)](.github/workflows/ci.yml)
[![Java JNI](https://img.shields.io/badge/Java-JNI%20binding-ED8B00?logo=openjdk&logoColor=white)](#java-绑定)
[![Powered by OrcaRouter](https://img.shields.io/badge/Powered_by-OrcaRouter-2563eb)](https://www.orcarouter.ai/ref/ref_a19e06c4695c9bfa903a)

> 🚀 **AI 模型 API 服务推荐：** [通过专属推广链接访问 OrcaRouter](https://www.orcarouter.ai/ref/ref_a19e06c4695c9bfa903a)

ComtradeCore 是一个轻量、Header-only 的 C++17 COMTRADE 库，用于构造、写入和流式读取电力系统暂态记录。
核心功能无第三方库依赖；CFG 中文编码转换在 Linux/macOS 使用系统 `iconv`，在 Windows 使用系统编码 API。

## 功能概览

- 使用 `Record` 在内存中读取、编辑和生成 CFG、DAT 文件。
- 使用 `StreamWriter` 写入 ASCII、BINARY、BINARY32 和 FLOAT32 DAT 数据。
- 使用 `StreamReader` 逐采样读取 ASCII、BINARY、BINARY32 和 FLOAT32 DAT，避免一次性加载完整文件。
- 自动移除 UTF-8 CFG 和 ASCII DAT 文件开头的 BOM，并将 GB2312、GBK、GB18030 CFG 的中文元数据统一转换为 UTF-8。
- 生成的 CFG 和 ASCII DAT 在所有平台统一使用 COMTRADE CRLF 行结束符。
- 支持 IEEE C37.111 的 1991、1999 和 2013 版本标识。
- 提供可安装的 CMake package，安装后可通过 `find_package()` 使用。

二进制 DAT 按 COMTRADE 小端布局处理：`BINARY` 使用 16 位有符号模拟量，`BINARY32` 使用 32 位
有符号模拟量，`FLOAT32` 使用 IEEE 754 单精度模拟量；数字量均按每 16 路一个状态字打包。

IEEE/IEC C37.111-2013 CFG 支持采样率段、时间倍率、小数秒精度、时间码、本地时区偏移、
时间质量码和闰秒指示；当 `nrates=0` 时，可读取并保持后续0到任意数量的采样率段。
下一版 PC37.111 仍在制定中，尚无可据以实现的正式发布版本。

## 环境要求

- 支持 C++17 的编译器：GCC、Clang 或 MSVC。
- CMake 3.14 或更高版本。
- Linux 和 macOS 需要系统提供 `iconv`（glibc 发行版和 macOS 通常已经内置）。
- GoogleTest 仅在构建单元测试时需要；核心库本身不依赖 GoogleTest。
- Java 绑定为可选组件；构建时还需要 SWIG 4.0 或更高版本、JDK（包含 JNI 头文件）和 Java 编译器。

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

## Java 绑定

Java 绑定提供 `comtrade.ComtradeRecord`、`ComtradeStreamReader` 和 `ComtradeStreamWriter`，覆盖内存型
记录以及恒定内存流式读写。它由一个 JAR 和一个 JNI 动态库组成：Java 项目在编译期引入 JAR，运行时
还必须能够找到当前平台对应的动态库。
它不是一个不依赖本地代码的纯 Java JAR。

### 为什么独立封装 Java API

独立封装并不是 JNI 的强制要求，但直接把全部 C++ 头文件交给 SWIG 会向 Java 用户暴露
`std::vector`、引用、指针、析构和回调等 C++ 细节。当前绑定因此采用分层结构：

```text
ComtradeCore C++ 核心
        ↓
bindings/java/java_api.hpp（转换为 SWIG 友好的类型和接口）
        ↓
SWIG 生成的 ComtradeNative* JNI 代理
        ↓
ComtradeRecord / ComtradeStreamReader / ComtradeStreamWriter
```

最外层 Java 门面主要负责：

- 使用 `double[]`、`boolean[]`、Java 枚举等自然类型代替 SWIG 容器；
- 使用 `AutoCloseable` 和 `try-with-resources` 明确释放底层 C++ 对象；
- 将 C++ 流式回调转换为可直接使用 Java Lambda 的 `RowHandler`；
- 集中处理空值、对象状态、异常和 JNI 动态库加载；
- 隔离 SWIG 生成代码，使内部映射变化时尽量不影响业务代码。

因此业务项目应优先使用 `comtrade.ComtradeRecord`、`ComtradeStreamReader` 和
`ComtradeStreamWriter`，不要直接依赖 `ComtradeNative*`、`DoubleVector` 或 `IntVector` 等生成类。

### 构建和安装

安装 SWIG 和 JDK 后启用 `BUILD_JAVA_BINDINGS`：

```bash
cmake -S . -B build-java \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_JAVA_BINDINGS=ON \
  -DCOMTRADE_BUILD_TESTS=OFF \
  -DCOMTRADE_BUILD_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX=/path/to/comtrade-install

cmake --build build-java --config Release
cmake --install build-java --config Release
```

在 CLion 的 CMake 配置中，把 `-DBUILD_JAVA_BINDINGS=ON` 添加到 **CMake 选项** 后重新加载项目。
远程 Linux 工具链必须在远程主机上安装 SWIG 和 JDK；CMake 使用的 JDK 应与最终运行 Java 程序的
CPU 架构一致。

安装后会得到：

```text
<prefix>/lib/comtrade/java/
├── comtrade-core-java.jar
└── libComtradeCoreJava.so       # Linux
```

Windows 对应的本地库名为 `ComtradeCoreJava.dll`，macOS 对应
`libComtradeCoreJava.dylib`。

### 在 Java 中直接引入

下面的程序创建一份 2013 版 ASCII COMTRADE 记录：

```java
import comtrade.ComtradeRecord;

public final class Example {
    public static void main(String[] args) {
        try (ComtradeRecord record = new ComtradeRecord()) {
            record.setStationAndDevice(
                    "GRID_01",
                    "RELAY_01",
                    ComtradeRecord.StandardVersion.V2013);
            record.setTimestamps(
                    "24/10/2019,04:59:47.000000000",
                    "24/10/2019,04:59:47.001000000");
            record.setTimestampFractionalDigits(9);
            record.set2013TimeMetadata("UTC", "+0", "F", 0);

            // COMTRADE 通道编号从 1 开始。
            record.addAnalogChannel(1, "VA", "A", "V", 0.1, -5.0);
            record.addDigitalChannel(1, "TRIP", "", false);

            record.addSample(0, new double[]{220.0}, new boolean[]{false});
            record.addSample(250, new double[]{221.2}, new boolean[]{true});
            record.save("record.cfg", "record.dat");
        }
    }
}
```

直接使用 `javac` 和 `java`：

```bash
javac -cp /path/to/comtrade-install/lib/comtrade/java/comtrade-core-java.jar Example.java
java \
  -Djava.library.path=/path/to/comtrade-install/lib/comtrade/java \
  -cp /path/to/comtrade-install/lib/comtrade/java/comtrade-core-java.jar:. \
  Example
```

Windows 的 classpath 分隔符应由 `:` 改为 `;`。Gradle 项目可直接加入本地 JAR：

```groovy
dependencies {
    implementation files('/path/to/comtrade-install/lib/comtrade/java/comtrade-core-java.jar')
}

tasks.withType(JavaExec).configureEach {
    systemProperty 'java.library.path', '/path/to/comtrade-install/lib/comtrade/java'
}
```

### Linux 下构建、部署与 Maven 引入

以下以 Ubuntu/Debian 为例。先安装 C++、SWIG、JDK 和 Maven：

```bash
sudo apt update
sudo apt install -y git build-essential cmake swig openjdk-17-jdk maven
```

如果使用系统中已经安装的其他完整 JDK（例如 OpenJDK 25）也可以。通过 `javac` 确认 JDK 根目录：

```bash
readlink -f "$(which javac)"
```

例如输出 `/usr/lib/jvm/java-25-openjdk-amd64/bin/javac` 时，可在当前终端设置：

```bash
export JAVA_HOME=/usr/lib/jvm/java-25-openjdk-amd64
export PATH="$JAVA_HOME/bin:$PATH"
```

从项目根目录执行完整 JNI 构建和安装：

```bash
cmake -S . -B build-java \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_JAVA_BINDINGS=ON \
  -DCOMTRADE_BUILD_TESTS=OFF \
  -DCOMTRADE_BUILD_EXAMPLES=OFF \
  -DCOMTRADE_BUILD_BENCHMARKS=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/install"

cmake --build build-java --parallel "$(nproc)"
cmake --install build-java
```

安装结果为：

```text
<ComtradeCore>/install/lib/comtrade/java/
├── comtrade-core-java.jar
└── libComtradeCoreJava.so
```

将 JAR 安装到当前 Linux 用户的 Maven 本地仓库；不要使用 `sudo mvn`，否则依赖会进入 root 用户的
`~/.m2`，普通用户构建项目时仍然无法找到：

```bash
mvn org.apache.maven.plugins:maven-install-plugin:3.1.4:install-file \
  -Dfile="$PWD/install/lib/comtrade/java/comtrade-core-java.jar" \
  -DgroupId=io.github.blacksnow1 \
  -DartifactId=comtrade-core-java \
  -Dversion=1.1.0.0 \
  -Dpackaging=jar \
  -DgeneratePom=true
```

在 Java 项目的 `pom.xml` 中加入：

```xml
<dependency>
    <groupId>io.github.blacksnow1</groupId>
    <artifactId>comtrade-core-java</artifactId>
    <version>1.1.0.0</version>
</dependency>
```

然后在 Java 项目根目录构建：

```bash
mvn -U clean package
```

运行时必须让 JVM 找到 JNI 动态库，并允许未命名模块访问本地代码：

> [!IMPORTANT]
> 在 IntelliJ IDEA 的 **VM options** 或 `java` 命令中，必须加入下面两个 JVM 参数：
>
> ```text
> --enable-native-access=ALL-UNNAMED -Djava.library.path=/home/wangguangbo/ComtradeCore/install/lib/comtrade/java
> ```
>
> `--enable-native-access=ALL-UNNAMED` 用于允许当前 JAR 中的 JNI 代码访问本地库，避免新版 JDK 的本地访问警告以及将来的访问阻止；`-Djava.library.path=...` 用于让 JVM 找到 `libComtradeCoreJava.so`。这些参数必须写在 `-jar` 或主类名称之前，不能放在程序参数（Program arguments）中。其他用户部署时，请将路径替换为自己的实际安装路径。

```bash
java \
  --enable-native-access=ALL-UNNAMED \
  -Djava.library.path=/home/wangguangbo/ComtradeCore/install/lib/comtrade/java \
  -jar target/your-application.jar
```

也可以为当前进程设置动态库搜索目录：

```bash
LD_LIBRARY_PATH=/home/wangguangbo/ComtradeCore/install/lib/comtrade/java \
  java --enable-native-access=ALL-UNNAMED -jar target/your-application.jar
```

检查 `.so` 是否缺少系统依赖：

```bash
ldd /absolute/path/to/ComtradeCore/install/lib/comtrade/java/libComtradeCoreJava.so
```

输出中没有 `not found` 即可。通过 Windows IDEA 的 SSH 运行目标部署到 Linux 时，应在 Maven 运行配置
中选择该 SSH 目标，并为目标添加 Linux JDK 与 Maven 运行时；Windows 与 Linux 的 Maven 本地仓库互不
共享。若需要让项目导入、代码索引、编译和运行全部使用 Linux 环境，建议通过 JetBrains Gateway 直接
打开远程 Linux 项目。

### Windows 下编译 Java 绑定

推荐使用 64 位 JDK 和 Visual Studio 2022 的 MSVC x64 工具链。编译前需要安装：

- Visual Studio 2022，并勾选 **使用 C++ 的桌面开发**；
- CMake；
- SWIG 4.0 或更高版本；
- 完整的 64 位 JDK（不能只安装 JRE）。

打开 PowerShell，确认这些命令都能被找到：

```powershell
cmake --version
swig -version
javac -version
java -version
```

如果尚未配置 `JAVA_HOME`，将它设置为 JDK 根目录。该目录下应直接包含 `bin\java.exe` 和
`bin\javac.exe`：

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-25"
$env:Path = "$env:JAVA_HOME\bin;$env:Path"
```

以上设置只对当前 PowerShell 窗口有效。请根据实际安装位置修改 JDK 路径，并确保 JDK、Visual Studio
生成目标和最终运行程序都是相同的 CPU 架构，例如均为 x64。

在 ComtradeCore 项目根目录执行：

```powershell
cmake -S . -B build-java-windows `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DBUILD_JAVA_BINDINGS=ON `
  -DCOMTRADE_BUILD_TESTS=OFF `
  -DCOMTRADE_BUILD_EXAMPLES=OFF `
  -DCOMTRADE_BUILD_BENCHMARKS=OFF `
  "-DCMAKE_INSTALL_PREFIX=$PWD\install-windows"

cmake --build build-java-windows --config Release --parallel
cmake --install build-java-windows --config Release
```

安装完成后会生成：

```text
<ComtradeCore>\install-windows\lib\comtrade\java\
├── comtrade-core-java.jar
└── ComtradeCoreJava.dll
```

#### 使用 Windows GCC（MinGW-w64）编译

Windows 下也可以使用 GCC，但必须使用面向 Windows 的 **MinGW-w64** 工具链，不能使用 WSL 中面向
Linux 的 GCC，也不要使用 Cygwin GCC。项目包含 C++ 代码，因此除了 `gcc` 还必须提供 `g++`。

在 PowerShell 中确认工具链可以被找到：

```powershell
gcc --version
g++ --version
mingw32-make --version
g++ -dumpmachine
```

建议 `g++ -dumpmachine` 的输出包含 `x86_64-w64-mingw32`，并使用同为 x64 的 JDK。将 MinGW-w64 的
`bin` 目录加入 `PATH` 后，在项目根目录执行：

```powershell
cmake -S . -B build-java-mingw `
  -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_JAVA_BINDINGS=ON `
  -DCOMTRADE_BUILD_TESTS=OFF `
  -DCOMTRADE_BUILD_EXAMPLES=OFF `
  -DCOMTRADE_BUILD_BENCHMARKS=OFF `
  "-DCMAKE_INSTALL_PREFIX=$PWD\install-windows-mingw"

cmake --build build-java-mingw --parallel
cmake --install build-java-mingw
```

不要让 MinGW 构建复用前面由 Visual Studio 生成的 `build-java-windows` 目录；CMake 构建目录只能对应
一个生成器和一套编译器。MinGW 安装结果位于：

```text
<ComtradeCore>\install-windows-mingw\lib\comtrade\java\
├── comtrade-core-java.jar
└── ComtradeCoreJava.dll
```

MinGW 生成的 `ComtradeCoreJava.dll` 还可能依赖以下 GCC 运行库，具体名称取决于所安装的 MinGW-w64
版本和线程模型：

```text
libstdc++-6.dll
libgcc_s_seh-1.dll
libwinpthread-1.dll
```

可以检查实际依赖：

```powershell
objdump -p .\install-windows-mingw\lib\comtrade\java\ComtradeCoreJava.dll | Select-String "DLL Name"
```

运行 Java 程序时，需要保持 MinGW-w64 的 `bin` 目录位于 `PATH` 中，或者将上述实际依赖的运行库
DLL 与 `ComtradeCoreJava.dll` 一起部署。Java 的 VM options 对应改为：

```text
--enable-native-access=ALL-UNNAMED -Djava.library.path=D:\GitHubProjects\ComtradeCore\install-windows-mingw\lib\comtrade\java
```

如果使用前面的 Visual Studio 构建产物，并在 Windows 本机运行 Java 程序，应把以下内容加入
IntelliJ IDEA 的 **VM options**；路径需按项目实际位置修改：

```text
--enable-native-access=ALL-UNNAMED -Djava.library.path=D:\GitHubProjects\ComtradeCore\install-windows\lib\comtrade\java
```

也可以直接从 PowerShell 启动：

```powershell
java --enable-native-access=ALL-UNNAMED `
  "-Djava.library.path=D:\GitHubProjects\ComtradeCore\install-windows\lib\comtrade\java" `
  -cp "D:\GitHubProjects\ComtradeCore\install-windows\lib\comtrade\java\comtrade-core-java.jar;." `
  org.example.Main
```

> [!IMPORTANT]
> Windows 生成的 `ComtradeCoreJava.dll` 只能在 Windows 上使用，不能复制到 Linux 运行。Linux 必须
> 单独编译并使用 `libComtradeCoreJava.so`；JAR 可以跨平台复用，但本地动态库必须与操作系统和 CPU
> 架构匹配。

### Windows 下引入 JAR

将构建产物复制到 Windows Java 项目，例如：

```text
D:\testjavaInterface\libs\
├── comtrade-core-java.jar
└── native\
    └── ComtradeCoreJava.dll
```

如果项目使用 Maven，可在 PowerShell 中把 JAR 安装到当前 Windows 用户的本地仓库：

```powershell
mvn org.apache.maven.plugins:maven-install-plugin:3.1.4:install-file `
  "-Dfile=D:\testjavaInterface\libs\comtrade-core-java.jar" `
  "-DgroupId=io.github.blacksnow1" `
  "-DartifactId=comtrade-core-java" `
  "-Dversion=1.1.0.0" `
  "-Dpackaging=jar" `
  "-DgeneratePom=true"
```

安装位置默认为：

```text
%USERPROFILE%\.m2\repository\io\github\blacksnow1\comtrade-core-java\1.1.0.0\
```

随后在项目的 `pom.xml` 中声明：

```xml
<dependency>
    <groupId>io.github.blacksnow1</groupId>
    <artifactId>comtrade-core-java</artifactId>
    <version>1.1.0.0</version>
</dependency>
```

重新加载 Maven 项目后即可编译。若程序也在 Windows 本机运行，还需在 IDEA 的 **VM options** 中添加：

```text
-Djava.library.path=D:\testjavaInterface\libs\native
```

并确保该目录包含与当前 JVM CPU 架构一致的 `ComtradeCoreJava.dll`。如果 Windows IDEA 只负责编译、
程序通过 SSH 目标在 Linux 运行，则 Windows 侧只需安装 JAR；远程运行配置应使用 Linux 的
`libComtradeCoreJava.so` 及其 Linux 路径，不能使用 Windows DLL。

### 发布到 Maven 仓库

CMake 生成的 JAR 不是由 Maven 项目构建的，因此可以使用 Maven Install Plugin 将它发布到当前用户的
本地仓库（默认是 `~/.m2/repository`）：

```bash
mvn org.apache.maven.plugins:maven-install-plugin:3.1.4:install-file \
  -Dfile=/path/to/comtrade-install/lib/comtrade/java/comtrade-core-java.jar \
  -DgroupId=io.github.blacksnow1 \
  -DartifactId=comtrade-core-java \
  -Dversion=1.1.0.0 \
  -Dpackaging=jar \
  -DgeneratePom=true
```

发布成功后，Maven 项目的 `pom.xml` 可以直接声明：

```xml
<dependency>
    <groupId>io.github.blacksnow1</groupId>
    <artifactId>comtrade-core-java</artifactId>
    <version>1.1.0.0</version>
</dependency>
```

如果需要发布到 Nexus、Artifactory 等远程 Maven 私服，先在当前用户的 `~/.m2/settings.xml` 中配置
认证信息。`server` 的 `id` 必须与后续命令的 `repositoryId` 相同；用户名和密码不要提交到仓库：

```xml
<settings xmlns="http://maven.apache.org/SETTINGS/1.2.0">
    <servers>
        <server>
            <id>internal-releases</id>
            <username>${env.MAVEN_REPOSITORY_USER}</username>
            <password>${env.MAVEN_REPOSITORY_PASSWORD}</password>
        </server>
    </servers>
</settings>
```

然后使用 Maven Deploy Plugin 发布：

```bash
mvn org.apache.maven.plugins:maven-deploy-plugin:3.1.4:deploy-file \
  -Dfile=/path/to/comtrade-install/lib/comtrade/java/comtrade-core-java.jar \
  -DgroupId=io.github.blacksnow1 \
  -DartifactId=comtrade-core-java \
  -Dversion=1.1.0.0 \
  -Dpackaging=jar \
  -DrepositoryId=internal-releases \
  -Durl=https://maven.example.com/repository/maven-releases/
```

上述 Maven 操作只发布 Java JAR，不会自动发布或加载 JNI 动态库。仍需把 Linux 的
`libComtradeCoreJava.so`、Windows 的 `ComtradeCoreJava.dll` 或 macOS 的
`libComtradeCoreJava.dylib` 部署到运行机器，并在 IDEA 的 **VM options** 或 Java 启动命令中指定：

```text
-Djava.library.path=/absolute/path/to/native
```

以上配置参考 Apache Maven 官方的
[Install Plugin `install-file`](https://maven.apache.org/plugins/maven-install-plugin/install-file-mojo.html)、
[Deploy Plugin `deploy-file`](https://maven.apache.org/plugins/maven-deploy-plugin/deploy-file-mojo.html) 和
[`settings.xml` 配置参考](https://maven.apache.org/settings.html)。

读取已有文件时调用 `record.load(cfgPath, datPath)`，它会根据 CFG 自动解析 ASCII、BINARY、BINARY32
或 FLOAT32；CFG 和 ASCII DAT 开头的 UTF-8 BOM 也会自动移除。随后可通过 `getSampleCount()`、
`getTimestampMicroseconds()`、`getAnalogValue()` 和 `getDigitalValue()` 访问数据。Java 门面也提供完整
通道字段、采样率段、CFG 时间信息及其修改接口；读取值时的通道位置和采样位置均从 0 开始。

大文件可使用 `ComtradeStreamReader.processDatStream()` 逐行回调；写入时可从当前
`ComtradeRecord` 配置快照创建 `ComtradeStreamWriter`，它支持 ASCII、BINARY、BINARY32 和 FLOAT32。

### Java 流式读取示例

流式读取会同步逐采样执行回调，不会将整份 DAT 文件加载到内存中，适合处理大文件。读取器会根据
CFG 的 DAT 类型自动处理 ASCII、BINARY、BINARY32 或 FLOAT32，不需要调用不同的方法。

```java
package org.example;

import comtrade.ComtradeStreamReader;

import java.util.Arrays;

public final class Main {
    public static void main(String[] args) {
        String cfgPath = "/home/wangguangbo/ComtradeFiles/cometrade/000002.CFG";
        String datPath = "/home/wangguangbo/ComtradeFiles/cometrade/000002.DAT";

        try (ComtradeStreamReader reader = new ComtradeStreamReader(cfgPath)) {
            System.out.println("站点：" + reader.getStationName());
            System.out.println("设备：" + reader.getDeviceId());
            System.out.println("DAT 类型：" + reader.getDataType());
            System.out.println("模拟量通道数：" + reader.getAnalogChannelCount());
            System.out.println("数字量通道数：" + reader.getDigitalChannelCount());

            long processed = reader.processDatStream(datPath, row -> {
                // 每收到一个采样行就立即打印。
                System.out.printf(
                        "采样=%d, 原始时间戳=%d, 时间=%s, 模拟量=%s, 数字量=%s%n",
                        row.index,
                        row.rawTimestamp,
                        row.absoluteTime,
                        Arrays.toString(row.analogValues),
                        Arrays.toString(row.digitalValues));
            });

            System.out.println("流式读取完成，共处理 " + processed + " 个采样");
        }
    }
}
```

在 IntelliJ IDEA 中运行时，把下面内容放入运行配置的 **VM options**：

```text
--enable-native-access=ALL-UNNAMED -Djava.library.path=/home/wangguangbo/ComtradeCore/install/lib/comtrade/java
```

`reader.getDataType()` 可用于记录或校验实际编码；四种 DAT 类型使用相同的回调数据结构，回调中的
模拟量已经应用 CFG 的 `a`、`b` 系数，数字量已经从二进制状态字展开。

### Java 中的 AI 分析（可选）

本仓库的“C++ AI 录波分析”模块（见文末对应章节）同步封装了 Java 接口：`comtrade.ComtradeAI`。它只有在构建
Java 绑定时**同时**打开 `BUILD_JAVA_BINDINGS=ON` 和 `COMTRADE_BUILD_AI=ON` 才会出现在
`comtrade-core-java.jar` 里；只打开 `BUILD_JAVA_BINDINGS` 不会让 Java 绑定意外依赖
nlohmann/json、cpp-httplib、OpenSSL 或 libharu：

```bash
cmake -S . -B build-java-ai \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_JAVA_BINDINGS=ON \
  -DCOMTRADE_BUILD_AI=ON \
  -DCOMTRADE_BUILD_TESTS=OFF \
  -DCOMTRADE_BUILD_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX=/path/to/comtrade-install

cmake --build build-java-ai --config Release
cmake --install build-java-ai --config Release
```

`comtrade.ComtradeAI` 的每个方法都直接收发 JSON 字符串，和 `comtrade-ai` 命令行工具的文件式用法
完全一致：`configJson` 就是 `config/ai-config.example.json` 里那份配置的 JSON 文本，
`evidenceJson` 就是 `summarize()` 返回的 JSON 文本。所有方法都是静态方法，没有需要关闭的原生对象：

```java
package org.example;

import comtrade.ComtradeAI;

import java.nio.file.Files;
import java.nio.file.Path;

public final class Main {
    public static void main(String[] args) throws Exception {
        String configJson = Files.readString(Path.of("ai-config.json"));

        // 仅本地汇总，不访问网络。
        String evidenceJson = ComtradeAI.summarize("sample.cfg", "sample.dat", configJson);

        // 调用配置的模型，再生成中文 PDF 报告。
        String interpretation = ComtradeAI.analyze(evidenceJson, configJson);
        ComtradeAI.exportPdf("report.pdf", evidenceJson, interpretation, configJson);
    }
}
```

`ComtradeAI.validateConfig(configJson, requireNetwork)` 可以在调用 `analyze()` 之前单独校验配置
是否完整（`requireNetwork=true` 时还会要求 `endpoint`/`model` 有效）。配置字段、安全边界、超时和
字节预算规则与 C++ 版本完全相同，详见 [docs/ai-analysis.md](docs/ai-analysis.md)。

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
`std::runtime_error`。它会读取 CFG 中声明的 DAT 类型，并自动选择 ASCII、BINARY、BINARY32 或
FLOAT32 解码路径。

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

### 多段不同采样率的录波

IEEE C37.111-2013 允许一份录波包含多段不同采样率（`cfg.sample_rates`，每段记录标称采样率和该段
最后一个物理采样点的累计序号）。逐点流式处理这类录波时，如果只按固定时长分窗口或分批，无法知道
"这一批点里混进了几种采样率"；`SampleRow` 因此直接带上了两个字段，免去调用方自己按 `end_sample`
做二分查找：

```cpp
reader.processDatStream("record.dat", [](const comtrade::SampleRow& row) {
    // sample_rates 的下标，从 0 开始；没有采样段信息时固定为 0。
    row.segment_index;
    // 当前采样点所属采样段的标称采样率（Hz）；没有采样段信息时为 0。
    row.segment_sample_rate;
});
```

`StreamReader` 按物理采样序号（第一个有效点为 1）在 `cfg().sample_rates` 中单调向后定位当前
采样段：DAT 本身就是严格按序产生的，所以定位游标只会前移，不会倒退，整份文件的定位总代价是
O(采样段数)，不会随采样点数增长，不影响流式处理原有的常数级内存占用。典型用法是在段切换时结束
当前批次、按新的 `segment_sample_rate` 重新决定窗口大小或重采样参数，例如：

```cpp
std::size_t current_segment = 0;
reader.processDatStream("record.dat", [&](const comtrade::SampleRow& row) {
    if (row.segment_index != current_segment) {
        current_segment = row.segment_index;
        // 在这里结束上一段的处理（比如落盘一个窗口），再按新的 segment_sample_rate 继续。
    }
    // ...按当前段处理 row...
});
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

### 流式读取基准测试结果

以下结果使用 `tests/ComtradeFiles/SIMENS/20191024045947.CFG` 和同名 DAT 文件，
在 Ubuntu 26.04 LTS 上分别使用 Release 和 Debug 模式连续读取 100 次得到。

测试环境：

| 项目 | 测试平台信息 |
| --- | --- |
| 操作系统 | Ubuntu 26.04 LTS (Resolute Raccoon) |
| CPU 型号 | 12th Gen Intel(R) Core(TM) i5-12400 |
| CPU 架构 | x86_64 |
| CPU 核心 / 线程 | 6 核 / 12 线程 |
| 内存 | 7.0 GiB（测试时可用 5.7 GiB） |
| 交换空间 | 4.0 GiB |
| 构建类型 | Release、Debug |

测试结果：

| 指标 | Release | Debug |
| --- | ---: | ---: |
| 循环次数 | 100 | 100 |
| 每轮采样数 | 1740 | 1740 |
| 总处理采样数 | 174000 | 174000 |
| 总耗时 | 0.326 s | 1.839 s |
| 数据吞吐量 | 99.546 MiB/s | 17.644 MiB/s |
| 采样处理速度 | 533806.786 samples/s | 94616.556 samples/s |
| 单采样平均耗时 | 1873.337 ns（约 1.873 us） | 10568.975 ns（约 10.569 us） |
| 校验和 | 189267714606.961 | 189267714606.961 |

数据吞吐量按照基准程序在正式计时区间内处理的 DAT 逻辑字节数计算，不包含 CFG 文件，也不包含
计时前的一次预热读取：

```text
总逻辑读取字节数 = DAT 文件大小（bytes）× 循环次数
总逻辑读取量（MiB） = 总逻辑读取字节数 ÷ 1024 ÷ 1024
数据吞吐量（MiB/s） = 总逻辑读取量（MiB）÷ 正式循环总耗时（s）
```

本次 DAT 文件大小为 `340244 bytes`，Release 模式循环 100 次，因此：

```text
总逻辑读取量 = 340244 × 100 ÷ 1024 ÷ 1024
              ≈ 32.448 MiB

数据吞吐量 = 32.448 ÷ 0.325962
           ≈ 99.546 MiB/s
```

输出中的 `elapsed_seconds=0.326` 只显示三位小数，而吞吐量使用未四舍五入的内部计时值计算，所以直接
使用显示出来的 `0.326` 反算时会有轻微差异。由于 DAT 在正式计时前已经预热，后续读取通常会命中操作
系统文件缓存；因此该指标主要反映“读取、文本解析、数值换算和回调消费”的综合速度，不代表存储设备的
物理顺序读取带宽。

本次结果表明，在上述 Linux x86_64 环境中，流式读取器平均每轮读取 1740 个采样点约需
3.26 ms（Release）或 18.39 ms（Debug）。Release 的吞吐量约为 Debug 的 5.64 倍，因此性能回归
比较应使用 Release 构建。两种构建模式得到相同的 `checksum`，说明 benchmark 消费到的结果一致；
该值用于防止读取结果被编译器无效优化并检查多轮结果是否稳定，不代表 COMTRADE 数据本身的业务含义。
性能数据会受到 CPU 动态调频、系统负载、编译器版本和存储介质影响，因此应在相同环境和构建类型下
比较不同版本。

### GitHub Actions 自动测试

`.github/workflows/ci.yml` 会在推送到 `main`、创建或更新 Pull Request，以及手动触发时运行。
自动化流程覆盖 Linux GCC、Linux Clang、macOS Apple Clang 和 Windows MSVC，并执行：

1. 配置和编译库、示例及 GoogleTest 测试。
2. 通过 CTest 运行全部单元测试。
3. 安装 ComtradeCore 到临时 staging 目录。
4. 使用独立消费者项目验证 `find_package(ComtradeCore CONFIG REQUIRED)`。
5. 上传保留 7 天的安装包构件。

真实 COMTRADE 文件的准确性测试和流式读取基准方法见
[`tests/README.md`](tests/README.md)。常规 CI 会选择性下载一组 Git LFS 测试文件并运行准确性测试；
`.github/workflows/benchmark.yml` 每周或手动运行 Release/GCC 基准，并保存性能结果 30 天。

## 目录结构

以下列出仓库中的主要源码与文档；测试数据目录仅展示入口。

```text
ComtradeCore/
├── .github/
│   ├── install-test/              # CMake 安装集成验证
│   └── workflows/                # CI 与基准测试工作流
├── benchmarks/
│   ├── CMakeLists.txt
│   └── stream_read_benchmark.cpp  # 流式读取性能基准
├── bindings/java/
│   ├── CMakeLists.txt             # JNI 动态库与 JAR 构建、安装
│   ├── comtrade.i                 # SWIG 接口与映射配置
│   ├── java_api.hpp               # Java 绑定的 C++ 适配层
│   ├── ai_java_api.hpp            # AI 模块的 Java 适配层（可选，随 COMTRADE_BUILD_AI 一起启用）
│   └── src/main/java/comtrade/
│       ├── ComtradeRecord.java
│       ├── ComtradeStreamReader.java
│       ├── ComtradeStreamWriter.java
│       └── ComtradeAI.java        # 可选：只在启用 AI 模块时随 JAR 一起构建
├── cmake/
│   └── ComtradeCoreConfig.cmake.in # CMake package 配置模板
├── docs/
│   └── swig-jni-java-binding-guide.md # SWIG/JNI 映射教程
├── examples/
│   ├── CMakeLists.txt
│   ├── example_high_perf_write.cpp # 高性能写入示例
│   └── record_lifecycle.cpp       # Record 生命周期示例
├── include/comtrade/
│   ├── binary_io.hpp              # 二进制 DAT 布局与小端转换
│   ├── cfg_io.hpp                 # CFG 解析工具
│   ├── comtrade.hpp               # Record 统一入口
│   ├── record.hpp                 # 内存记录的构造、读写
│   ├── stream_reader.hpp          # 流式读取
│   ├── stream_writer.hpp          # 流式写入
│   ├── text_encoding.hpp          # 文本编码转换与 BOM 处理
│   ├── types.hpp                  # 数据类型和通道定义
│   ├── utils.hpp                  # 内部工具
│   ├── ai.hpp                     # AI 模块单一入口（可选，见下文），#include 下面 ai/ 里的头文件
│   └── ai/                        # AI 模块实现，与上面的核心读写实现物理分开的独立子目录
│       ├── config.hpp             # AI 配置：字段校验、fromJson/fromFile
│       ├── statistics.hpp         # AI 用统计累加器（RMS/均值/极值）
│       ├── evidence.hpp           # 本地 CFG/DAT 摘要为证据 JSON
│       ├── client.hpp             # HTTPS 调用 Chat Completions 模型
│       ├── pdf.hpp                # 中文 PDF 报告渲染
│       └── detail.hpp             # AI 模块内部工具，非公开 API
├── analysis/cpp/
│   ├── CMakeLists.txt             # AI 依赖获取、comtrade::ComtradeAI/CLI 定义、AI 专属安装规则
│   └── src/main.cpp               # comtrade-ai 命令行工具（使用 include/comtrade/ai.hpp）
├── config/
│   └── ai-config.example.json     # AI 模块配置示例
├── tests/
│   ├── ComtradeFiles/             # 真实 COMTRADE 测试数据
│   ├── CMakeLists.txt
│   ├── README.md                  # 测试与基准方法说明
│   ├── test_real_files.cpp        # 真实文件兼容性测试
│   ├── test_record.cpp            # 内存记录测试
│   ├── test_stream.cpp            # 流式读写测试
│   ├── test_ai.cpp                # AI 模块 GoogleTest 单测（可选，随 COMTRADE_BUILD_AI 构建）
│   └── verify_pdf.py              # 用 Poppler 独立校验 AI 生成的中文 PDF
├── .gitattributes
├── .gitignore
├── CMakeLists.txt                 # 顶层构建与安装配置
├── LICENSE
└── README.md
```

`build-*`、`install*` 等目录是本地构建或安装时生成的产物，不属于上面的源码结构。
SWIG 生成的 Java 代理和 C++ JNI 包装代码位于所选构建目录的 `bindings/java/generated/` 下。

## C++ AI 录波分析（可选）

`include/comtrade/ai.hpp` + `include/comtrade/ai/` 是纯 C++17、**header-only** 的录波摘要、模型
分析和中文 PDF 报告模块，实现代码单独放在 `ai/` 子目录里，与核心 COMTRADE 读写实现物理分开，
不含任何需要单独编译的 `.cpp` 实现；`analysis/cpp` 只放获取依赖的 CMake 脚本和使用这些头文件的
`comtrade-ai` 命令行工具。依赖 cpp-httplib/OpenSSL、nlohmann/json 与 libharu，无 Qt、Java/JNI 依赖。
默认既不构建也不安装，不增加核心库依赖；是否构建（`COMTRADE_BUILD_AI`）和是否安装
（`COMTRADE_INSTALL_AI`）是两个独立的 CMake 选项，详见
[构建、配置与测试说明、每个配置字段的含义和依赖版本](docs/ai-analysis.md)。

```sh
cmake -S . -B build-ai -DCOMTRADE_BUILD_AI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-ai --config Release --parallel 2
ctest --test-dir build-ai -C Release --output-on-failure
```

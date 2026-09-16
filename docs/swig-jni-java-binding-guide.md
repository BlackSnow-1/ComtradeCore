# 从 C++ 到 Java：用 SWIG 和 JNI 构建可维护的跨语言接口

> 本文面向希望把现有 C++ 库提供给 Java 使用的开发者。文章不依赖某个具体业务，重点解释 Java、JNI、SWIG、C++ 适配层之间的关系，以及一套可以迁移到多数项目中的接口设计规则。

## 一、为什么需要 JNI 和 SWIG

Java 代码运行在 JVM 中，C++ 代码被编译为本机机器码，两者拥有不同的类型系统、对象模型、异常机制和内存管理方式。Java 不能直接实例化一个 C++ 类，也不能直接调用 C++ 成员函数。

JNI（Java Native Interface）提供了 JVM 与本机代码交互的标准机制。理论上，我们可以为每个 Java 方法手写一个 `JNIEXPORT` 函数，但接口数量增加后，通常会出现以下问题：

- Java 方法与 JNI 函数名称很长，维护成本高；
- 字符串、数组和异常转换存在大量重复代码；
- C++ 类指针的创建、保存和销毁容易出错；
- Java 或 C++ 接口调整后，两侧代码容易失去同步；
- 回调、继承和容器映射需要编写复杂的胶水代码。

SWIG（Simplified Wrapper and Interface Generator）位于 JNI 之上。它读取 C++ 声明和接口描述文件，自动生成 JNI C++ 包装代码和 Java 代理类。开发者仍然需要设计跨语言边界，但不必手工实现每一个 JNI 函数。

一个可维护的工程通常采用四层结构：

```text
业务 Java API（手写、面向最终用户）
        ↓
SWIG Java 代理（自动生成）
        ↓ JNI
本机动态库中的 SWIG 包装代码（自动生成）
        ↓
C++ 适配层（手写、面向跨语言边界）
        ↓
C++ 核心库（已有业务实现）
```

这套架构的关键不是“让 SWIG 暴露所有 C++ 代码”，而是先设计一个适合 Java 的 C++ 适配层，再让 SWIG 映射这个适配层。

## 二、最小示例：让 Java 调用 C++ 加法函数

下面用一个计算器演示完整关系。

### 2.1 C++ 适配接口

创建 `native_calculator.hpp`：

```cpp
#pragma once

class NativeCalculator {
public:
    int add(int left, int right) const {
        return left + right;
    }
};
```

这里暂时没有复杂业务。`NativeCalculator` 同时承担 C++ 实现和跨语言适配接口的角色。在真实项目中，它通常会持有或调用另一个核心业务对象。

### 2.2 SWIG 接口文件

创建 `native_calculator.i`：

```swig
%module NativeCalculatorModule

%{
#include "native_calculator.hpp"
%}

%include "native_calculator.hpp"
```

同一个头文件出现了两次，但用途完全不同。

```swig
%{
#include "native_calculator.hpp"
%}
```

`%{ ... %}` 中的内容会被复制到 SWIG 生成的 C++ 包装文件，最终交给 C++ 编译器。SWIG 不会用这段内容生成 Java API。

```swig
%include "native_calculator.hpp"
```

`%include` 会让 SWIG 分析类和方法声明，并生成相应的 Java 代理与 JNI 包装。缺少这一行时，C++ 可能正常编译，但 Java 侧不会出现 `NativeCalculator`。

### 2.3 SWIG 生成的内容

生成结果大致包括：

```text
NativeCalculator.java
NativeCalculatorModule.java
NativeCalculatorModuleJNI.java
native_calculator_wrap.cxx
```

其中：

- `NativeCalculator.java` 是 C++ 类的 Java 代理；
- `NativeCalculatorModuleJNI.java` 声明真正的 Java `native` 方法；
- `native_calculator_wrap.cxx` 实现 JNI 函数并调用 C++；
- Java 代理内部通常使用一个 `long` 保存 C++ 对象地址。

可以把自动生成的代理简化理解为：

```java
public class NativeCalculator {
    private long nativePointer;

    public NativeCalculator() {
        nativePointer = NativeCalculatorModuleJNI.new_NativeCalculator();
    }

    public int add(int left, int right) {
        return NativeCalculatorModuleJNI.NativeCalculator_add(
                nativePointer, this, left, right);
    }

    public void delete() {
        NativeCalculatorModuleJNI.delete_NativeCalculator(nativePointer);
        nativePointer = 0;
    }
}
```

实际生成代码会包含所有权标志、同步保护等更多细节，不应该手工修改，因为下次运行 SWIG 时会被覆盖。

### 2.4 手写 Java 门面

不建议让业务代码直接依赖 SWIG 代理。可以再提供一个稳定、符合 Java 使用习惯的门面：

```java
package org.example.calculator;

public final class Calculator implements AutoCloseable {
    static {
        System.loadLibrary("NativeCalculator");
    }

    private NativeCalculator nativeCalculator = new NativeCalculator();

    public int add(int left, int right) {
        ensureOpen();
        return nativeCalculator.add(left, right);
    }

    @Override
    public void close() {
        if (nativeCalculator != null) {
            nativeCalculator.delete();
            nativeCalculator = null;
        }
    }

    private void ensureOpen() {
        if (nativeCalculator == null) {
            throw new IllegalStateException("Calculator is already closed");
        }
    }
}
```

业务代码只接触这个类：

```java
public final class Main {
    public static void main(String[] args) {
        try (Calculator calculator = new Calculator()) {
            System.out.println(calculator.add(10, 20));
        }
    }
}
```

一次调用的完整路径为：

```text
Main.main()
  → Calculator.add()
  → SWIG 生成的 NativeCalculator.add()
  → NativeCalculatorModuleJNI 中的 native 方法
  → native_calculator_wrap.cxx 中的 JNI 函数
  → C++ NativeCalculator::add()
  → 结果沿原路径返回 Java
```

## 三、为什么还需要 C++ 适配层

SWIG 可以解析大量 C++ 语法，但“能够生成代码”不代表生成的 Java API 易用或安全。真实 C++ 库经常包含：

- `std::chrono::time_point`；
- `std::vector<bool>`；
- 模板类；
- 裸指针和引用；
- 重载复杂的成员函数；
- 返回内部容器引用的访问器；
- 依赖调用方管理生命周期的回调；
- 只在 C++ 语义下成立的隐式转换。

直接暴露这些接口会把 C++ 实现细节带到 Java。因此推荐在核心库和 SWIG 之间增加一个适配层：

```cpp
class NativeRecord {
public:
    bool load(const std::string& config_path,
              const std::string& data_path) {
        return record_.load(config_path, data_path);
    }

    long long sampleCount() const {
        return static_cast<long long>(record_.sampleCount());
    }

    std::string startTime() const {
        return formatTime(record_.startTime());
    }

private:
    CoreRecord record_;
};
```

适配层应尽量只使用边界清晰的类型：

```text
bool
int
long long
double
std::string
std::vector<double>
std::vector<int>
```

核心库仍然可以使用复杂且高效的 C++ 类型，适配层只负责在跨语言边界上完成转换。

## 四、常用类型映射

| C++ 类型 | JNI 类型 | Java 类型 |
|---|---|---|
| `bool` | `jboolean` | `boolean` |
| `int` | `jint` | `int` |
| `long long` | `jlong` | `long` |
| `float` | `jfloat` | `float` |
| `double` | `jdouble` | `double` |
| `std::string` | `jstring` | `String` |
| `std::vector<double>` | SWIG 容器代理 | 自定义名称，例如 `DoubleVector` |
| `std::vector<int>` | SWIG 容器代理 | 自定义名称，例如 `IntVector` |

在 SWIG 接口文件中可以这样启用标准映射：

```swig
%include "std_string.i"
%include "std_vector.i"

namespace std {
    %template(DoubleVector) vector<double>;
    %template(IntVector) vector<int>;
}
```

### 4.1 不要直接映射 `std::vector<bool>`

`std::vector<bool>` 是 C++ 标准库的位压缩特化，不是普通的连续 `bool` 数组。代理引用、内存布局和模板行为都比较特殊。

更稳定的方案是在适配层转换为：

```cpp
std::vector<int> toIntegerFlags(const std::vector<bool>& values) {
    std::vector<int> result;
    result.reserve(values.size());
    for (bool value : values) {
        result.push_back(value ? 1 : 0);
    }
    return result;
}
```

Java 门面再将它转换为 `boolean[]`。

### 4.2 有符号与无符号整数

Java 没有 `uint32_t`、`uint64_t` 对应的基本类型。常见策略是使用更宽的 Java 有符号类型，并在 C++ 适配层检查范围：

```cpp
std::uint32_t checkedUint32(long long value) {
    if (value < 0 ||
        static_cast<unsigned long long>(value) >
            std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("value exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}
```

不要直接执行窄化转换，否则超出范围的 Java `long` 可能被静默截断。

### 4.3 时间类型

复杂的 `std::chrono` 类型通常不适合直接暴露。可以根据语义选择：

- 相对时间使用 Java `long`，并明确单位是纳秒、微秒还是毫秒；
- 绝对时间返回 ISO 8601 字符串；
- 或在 Java 门面中转换成 `Instant`、`Duration`。

接口名称最好带单位，例如：

```java
long getTimestampMicroseconds();
long getOffsetNanoseconds();
```

不要只写含义模糊的 `getTime()`。

## 五、异常映射

SWIG 可以通过标准异常映射文件处理常见 C++ 异常：

```swig
%include "std_except.i"
```

常见映射关系包括：

```text
std::invalid_argument → Java IllegalArgumentException
std::out_of_range     → Java IndexOutOfBoundsException
std::runtime_error    → Java RuntimeException
```

建议区分两类失败：

1. 文件不存在、格式错误等业务层可预期失败，可以返回 `bool` 或使用明确的业务异常；
2. 参数越界、对象已关闭、数值溢出等编程错误，应抛出异常。

跨 JNI 边界之前必须检查：

- 整数范围；
- 数组长度是否与通道数等配置一致；
- 浮点值是否为有限数；
- 传入索引是否合法；
- 指针或对象是否仍然有效。

不要让未知 C++ 异常穿过 JNI 边界。无法明确分类时，至少应转换成 Java `RuntimeException`。

## 六、C++ 回调如何进入 Java

普通调用的方向是 Java 调用 C++。流式处理、进度通知和事件监听则需要 C++ 反向调用 Java。

可以先在 C++ 适配层定义抽象回调：

```cpp
class NativeRowCallback {
public:
    virtual ~NativeRowCallback() = default;

    virtual void onRow(long long index,
                       const std::vector<double>& values) = 0;
};
```

在 SWIG 文件中启用 director：

```swig
%module(directors="1") NativeModule
%feature("director") NativeRowCallback;
```

Java 侧可以覆写这个虚函数：

```java
NativeRowCallback callback = new NativeRowCallback() {
    @Override
    public void onRow(long index, DoubleVector values) {
        // C++ 每产生一行数据，就会回调这里
    }
};
```

调用路径变为：

```text
Java 启动流式处理
  → JNI 进入 C++
  → C++ 解析一个数据块
  → 调用 NativeRowCallback::onRow()
  → SWIG director 通过 JNI 回到 Java
  → Java 执行业务回调
```

设计回调时需要明确：

- 回调是同步还是异步；
- 回调在哪个线程执行；
- 参数在回调结束后是否仍然有效；
- Java 回调抛出异常时是否终止本机处理；
- 谁负责销毁 callback 代理；
- 高频回调是否会产生过多临时对象。

如果 C++ 复用行缓冲，而 Java 用户需要保存数据，应在 Java 门面中复制成独立数组或不可变对象。

## 七、内存和对象生命周期

Java 使用垃圾回收，C++ 通常使用 RAII。JNI 对象同时跨越两种生命周期模型，必须明确所有权。

SWIG Java 代理通常保存：

```text
long swigCPtr
boolean swigCMemOwn
```

它们分别表示 C++ 指针和代理是否拥有该对象。Java GC 何时运行并不确定，因此不能只等待终结器释放重要资源。

推荐公开 Java 类实现 `AutoCloseable`：

```java
public final class NativeResource implements AutoCloseable {
    private GeneratedNativeProxy proxy = new GeneratedNativeProxy();

    @Override
    public void close() {
        if (proxy != null) {
            proxy.delete();
            proxy = null;
        }
    }
}
```

调用方使用：

```java
try (NativeResource resource = new NativeResource()) {
    // 使用本机对象
}
```

临时 SWIG 容器也要显式删除：

```java
DoubleVector nativeValues = toNative(values);
try {
    nativeObject.process(nativeValues);
} finally {
    nativeValues.delete();
}
```

不要返回指向 C++ 临时对象、局部变量或随下一次调用失效的引用。

## 八、手写 Java 门面的价值

SWIG 生成类适合做机械映射，不适合作为长期公共 API，原因包括：

- 类名和方法名可能受 C++ 命名影响；
- 用户会看到 `delete()`、native 指针和 SWIG 容器；
- `DoubleVector` 不如 `double[]` 或 `List<Double>` 符合 Java 习惯；
- C++ API 调整可能直接破坏 Java 使用方；
- 无法自然表达 `AutoCloseable`、Java 枚举、不可变值对象等语义。

手写门面可以提供：

```java
public DataType getDataType();
public double[] readValues();
public void process(RowHandler handler);
```

内部再转换为 SWIG 代理。这样即使 C++ 内部结构变化，只要门面行为保持一致，Java 用户通常不需要修改代码。

## 九、使用 CMake 构建

一个简化的 CMake 配置如下：

```cmake
cmake_minimum_required(VERSION 3.18)
project(NativeCalculator LANGUAGES CXX)

find_package(SWIG REQUIRED)
find_package(Java REQUIRED COMPONENTS Development)
find_package(JNI REQUIRED)

include(UseSWIG)

set_source_files_properties(native_calculator.i PROPERTIES
    CPLUSPLUS ON
)

swig_add_library(NativeCalculator
    TYPE SHARED
    LANGUAGE java
    SOURCES native_calculator.i
)

target_include_directories(NativeCalculator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JNI_INCLUDE_DIRS}
)

target_link_libraries(NativeCalculator PRIVATE
    ${JNI_LIBRARIES}
)

set_target_properties(NativeCalculator PROPERTIES
    CXX_STANDARD 17
    OUTPUT_NAME NativeCalculator
)
```

配置和构建：

```bash
cmake -S . -B build
cmake --build build --config Release
```

不同平台的动态库名称通常为：

```text
Linux:   libNativeCalculator.so
Windows: NativeCalculator.dll
macOS:   libNativeCalculator.dylib
```

Java 加载时只写基础名称：

```java
System.loadLibrary("NativeCalculator");
```

不要写 `.so`、`.dll`、`lib` 前缀或绝对路径。

运行时通过 `java.library.path` 指定动态库目录：

```bash
java \
  -Djava.library.path=/absolute/path/to/native \
  -cp application.jar:native-api.jar \
  org.example.Main
```

Windows 的 classpath 分隔符为分号：

```powershell
java `
  "-Djava.library.path=C:\path\to\native" `
  -cp "application.jar;native-api.jar" `
  org.example.Main
```

`classpath` 用于寻找 Java 类，`java.library.path` 用于寻找本机动态库，两者不是一回事。

## 十、JAR 和动态库的关系

JNI 项目通常至少包含两个产物：

```text
native-api.jar
libNativeCalculator.so / NativeCalculator.dll / libNativeCalculator.dylib
```

JAR 可以跨平台复用，但本机动态库不能跨平台：

- Windows JVM 需要 Windows DLL；
- Linux JVM 需要 Linux SO；
- macOS JVM 需要 macOS DYLIB；
- 动态库的 CPU 架构必须与 JVM 一致，例如都为 x86_64 或都为 arm64。

JAR 和动态库不必放在同一目录。只要 JAR 位于 classpath，动态库目录位于 `java.library.path` 即可。

将动态库打进 JAR 并不会让 JVM 自动加载它。如果希望提供单 JAR 体验，需要在运行时根据操作系统和架构选择资源，将动态库释放到临时目录，再调用 `System.load()`。这属于额外的分发层设计。

## 十一、增加新接口的通用流程

当需要增加一个跨语言方法时，可以按以下顺序处理。

### 第一步：先实现和测试 C++ 核心

跨语言层不应承担业务算法。先保证核心 C++ 方法可以独立测试。

### 第二步：在 C++ 适配层提供简单接口

```cpp
class NativeRecord {
public:
    long long durationMicroseconds() const {
        return checkedJavaLong(record_.duration());
    }
};
```

在这里完成范围检查、类型转换和异常规范化。

### 第三步：判断是否需要修改 SWIG 文件

- 给已有类增加基础类型方法：通常不需要修改 SWIG 文件；
- 增加新的 `std::vector<T>`：需要增加 `%template`；
- 增加新的回调类：需要增加 `%feature("director")`；
- 不希望暴露某个方法：使用 `%ignore`；
- 需要定制参数转换：编写或引用 typemap。

### 第四步：重新生成代理

不要手动修改 SWIG 生成文件。重新运行构建，让 SWIG 重新生成。

### 第五步：在 Java 门面中包装

```java
public long getDurationMicroseconds() {
    ensureOpen();
    return nativeRecord.durationMicroseconds();
}
```

### 第六步：更新构建文件

如果新增了 SWIG 类，需要把对应的生成 Java 文件加入 `javac` 输入。如果新增了手写 Java 门面，也需要加入 JAR 源文件清单。

### 第七步：增加跨语言测试

至少验证：

- 正常值；
- 空输入；
- 最大、最小整数；
- 数组长度不匹配；
- Java 对象关闭后再次调用；
- C++ 异常能否正确映射；
- 回调顺序和回调异常；
- 32/64 位和不同操作系统的动态库加载。

## 十二、常见错误

### 12.1 `UnsatisfiedLinkError: no xxx in java.library.path`

JVM 没有找到动态库。检查：

- `-Djava.library.path` 是否放在主类或 `-jar` 之前；
- 文件名是否符合当前平台规则；
- 路径是否为目录而不是动态库文件本身。

### 12.2 找到动态库但仍然无法加载

动态库可能缺少二级依赖，或者与 JVM 架构不一致。Linux 可使用：

```bash
ldd libNativeCalculator.so
```

Windows 可以使用 `dumpbin /dependents` 或 `objdump -p`。

### 12.3 `%{ #include ... %}` 已存在，但 Java 类没有生成

还需要：

```swig
%include "header.hpp"
```

前者只服务 C++ 编译，后者才让 SWIG 分析接口。

### 12.4 修改了生成的 Java 文件

下次构建会覆盖修改。公共行为应写在手写 Java 门面中，映射规则应写在 `.i` 文件或 C++ 适配层中。

### 12.5 Java 可以调用，但运行一段时间后内存上涨

通常是 native 对象或临时 SWIG 容器没有执行 `delete()`。为公开对象实现 `AutoCloseable`，为临时对象使用 `finally`。

### 12.6 直接暴露核心 C++ 数据结构

这会把 C++ ABI、容器、引用生命周期和内部实现绑定到 Java。应增加稳定的适配层，只暴露 Java 真正需要的数据。

## 十三、测试和 CI 建议

跨语言工程至少应分为三层测试：

1. **C++ 核心测试**：验证算法、解析和数据结构，不经过 JNI；
2. **C++ 适配层测试**：验证范围检查、类型转换和异常；
3. **Java 集成测试**：真正加载动态库并调用 JAR 中的 API。

CI 应至少覆盖：

```text
Linux + GCC/Clang + JDK
Windows + MSVC/MinGW + JDK
macOS + Apple Clang + JDK
```

构建成功并不等于 JNI 可以运行。Java 集成测试必须实际执行：

```java
System.loadLibrary(...);
new NativeObject();
nativeMethod();
close();
```

这样才能发现动态库命名、依赖、架构、JNI 符号和生命周期问题。

## 十四、什么时候适合使用 SWIG

SWIG 适合：

- 已有较大规模 C/C++ 库；
- 需要同时支持 Java、Python、C# 等语言；
- 接口相对稳定，数据模型可以通过适配层简化；
- 希望减少重复 JNI 胶水代码。

手写 JNI 可能更适合：

- 接口数量非常少；
- 对每次调用的性能和内存分配有极致要求；
- 需要深度使用 JNI 局部引用、直接缓冲区或 JVM 线程 API；
- 团队不希望引入代码生成工具。

即使使用 SWIG，也应该理解 JNI 的对象、线程、异常和内存模型。SWIG 能消除机械代码，但不能替代跨语言 API 设计。

## 十五、总结

一套可维护的 Java/C++ 绑定通常遵循以下原则：

1. 核心业务保留在独立的 C++ 库中；
2. 使用 C++ 适配层收敛复杂类型和生命周期；
3. 只让 SWIG 映射稳定、简单、明确的接口；
4. 使用手写 Java 门面隐藏生成代码；
5. 使用 `AutoCloseable` 显式管理 native 资源；
6. 对整数范围、数组长度、异常和回调进行边界检查；
7. 不手工修改 SWIG 生成文件；
8. 用真实 Java 进程执行跨平台集成测试。

最终目标不是简单地“让 Java 能调用 C++”，而是让 Java 用户获得一个自然、稳定、安全的 API，同时让 C++ 核心保持独立和可演进。

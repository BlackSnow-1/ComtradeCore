%module ComtradeCore
%{
#include "comtrade/record.hpp"
// 若有计算逻辑，可在此包含自定义头文件，例如二次谐波越限判断类的头文件
%}

// 自动转换 C++ 的标准类型到 Java
%include "std_string.i"
%include "std_vector.i"

// 暴露核心类供 Java 实例化
%include "comtrade/record.hpp"
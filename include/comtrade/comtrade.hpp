/**
* @file comtrade.hpp
 * @brief Header-only library for parsing and generating COMTRADE format files.
 * @author 王广博
 */
#pragma once

// 聚合入口：适合使用内存型 Record API 的调用方一次性包含公共数据类型、
// 字符串/时间工具和记录对象。流式读写器仍可按需单独包含，以减少无关依赖。
#include "types.hpp"
#include "utils.hpp"
#include "record.hpp"  // 引入统一的 Record 对象

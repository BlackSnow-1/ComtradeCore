/**
 * @file binary_io.hpp
 * @brief COMTRADE binary DAT row layout and little-endian conversion helpers.
 */
#pragma once

#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace comtrade {
namespace detail {

// COMTRADE binary DAT fields are stored least-significant byte first.  These
// helpers keep the on-disk representation independent of the host byte order.
inline std::uint16_t readUint16LittleEndian(const char*& cursor) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(cursor);
    cursor += 2;
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

inline std::uint32_t readUint32LittleEndian(const char*& cursor) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(cursor);
    cursor += 4;
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

inline std::int16_t readInt16LittleEndian(const char*& cursor) noexcept {
    const auto bits = readUint16LittleEndian(cursor);
    std::int16_t value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::int32_t readInt32LittleEndian(const char*& cursor) noexcept {
    const auto bits = readUint32LittleEndian(cursor);
    std::int32_t value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline float readFloat32LittleEndian(const char*& cursor) noexcept {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "FLOAT32 requires a 32-bit float type");
    const auto bits = readUint32LittleEndian(cursor);
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline void writeUint16LittleEndian(char*& cursor, const std::uint16_t value) noexcept {
    cursor[0] = static_cast<char>(value & 0xffU);
    cursor[1] = static_cast<char>((value >> 8U) & 0xffU);
    cursor += 2;
}

inline void writeUint32LittleEndian(char*& cursor, const std::uint32_t value) noexcept {
    cursor[0] = static_cast<char>(value & 0xffU);
    cursor[1] = static_cast<char>((value >> 8U) & 0xffU);
    cursor[2] = static_cast<char>((value >> 16U) & 0xffU);
    cursor[3] = static_cast<char>((value >> 24U) & 0xffU);
    cursor += 4;
}

inline void writeInt16LittleEndian(char*& cursor, const std::int16_t value) noexcept {
    std::uint16_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    writeUint16LittleEndian(cursor, bits);
}

inline void writeInt32LittleEndian(char*& cursor, const std::int32_t value) noexcept {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    writeUint32LittleEndian(cursor, bits);
}

inline void writeFloat32LittleEndian(char*& cursor, const float value) noexcept {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "FLOAT32 requires a 32-bit float type");
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    writeUint32LittleEndian(cursor, bits);
}

inline bool isBinaryDataType(const DataType data_type) noexcept {
    return data_type == DataType::BINARY || data_type == DataType::BINARY32 ||
           data_type == DataType::FLOAT32;
}

inline std::size_t binaryAnalogWidth(const DataType data_type) {
    if (data_type == DataType::BINARY) return 2U;
    if (data_type == DataType::BINARY32 || data_type == DataType::FLOAT32) return 4U;
    throw std::invalid_argument("ASCII DAT does not have a binary analog width");
}

inline std::size_t digitalWordCount(const std::size_t digital_count) noexcept {
    return (digital_count + 15U) / 16U;
}

inline std::size_t binaryRowSize(const DataType data_type,
                                 const std::size_t analog_count,
                                 const std::size_t digital_count) {
    // Every binary row starts with a 32-bit sample number and timestamp.
    return 8U + analog_count * binaryAnalogWidth(data_type) +
           digitalWordCount(digital_count) * 2U;
}

}  // namespace detail
}  // namespace comtrade

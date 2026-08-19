#include <comtrade/stream_reader.hpp>
#include <comtrade/types.hpp>

#include <string>
#include <type_traits>

static_assert(!std::is_default_constructible_v<comtrade::StreamReader>);

int main() {
    const auto type_name = comtrade::DataTypeUtils::ToString(comtrade::DataType::FLOAT32);
    return type_name == std::string("FLOAT32") ? 0 : 1;
}

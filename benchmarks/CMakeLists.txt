add_executable(comtrade_stream_benchmark stream_read_benchmark.cpp)
target_link_libraries(comtrade_stream_benchmark PRIVATE comtrade::ComtradeCore)
target_compile_definitions(comtrade_stream_benchmark PRIVATE
        COMTRADE_BENCHMARK_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/ComtradeFiles"
)

# example/CMakeLists.txt

# 1. 查找当前目录下所有的 .cpp 文件，存入 EXAMPLE_SOURCES 变量中
file(GLOB EXAMPLE_SOURCES "*.cpp")

# 2. 遍历每一个找出的 .cpp 文件
foreach(source_file IN LISTS EXAMPLE_SOURCES)

    # 提取不带后缀的文件名。例如: "high_perf_write.cpp" 提取出 "high_perf_write"
    get_filename_component(target_name "${source_file}" NAME_WE)

    # 为了避免和根目录其他目标重名，可以加个前缀 (可选)
    set(exe_name "example_${target_name}")

    # 自动创建可执行文件
    add_executable(${exe_name} "${source_file}")

    # 自动链接核心库 (假设你的 Header-only target 叫 ComtradeCore)
    target_link_libraries(${exe_name} PRIVATE ComtradeCore)

    # 统一输出到 build/bin 目录下
    set_target_properties(${exe_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

endforeach()
cmake_minimum_required(VERSION 3.14)
project(ComtradeAIInstallTest LANGUAGES CXX)
find_package(ComtradeCore 1.2 CONFIG REQUIRED COMPONENTS AI)
add_executable(ai_consumer main.cpp)
target_link_libraries(ai_consumer PRIVATE comtrade::ComtradeAI)
enable_testing()
add_test(NAME InstalledAIConsumer COMMAND ai_consumer)
add_test(NAME InstalledAICli COMMAND ${CMAKE_COMMAND}
    -DCLI=$<TARGET_FILE:comtrade::comtrade-ai>
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-test
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cli-test.cmake)

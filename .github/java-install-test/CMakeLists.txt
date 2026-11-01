cmake_minimum_required(VERSION 3.18)
project(ComtradeJavaInstalledConsumer LANGUAGES NONE)
find_package(Java 11 REQUIRED COMPONENTS Development Runtime)
set(COMTRADE_JAVA_DIR "" CACHE PATH "Installed directory containing the JAR and JNI library")
option(COMTRADE_EXPECT_AI "Require and exercise the installed Java AI API" OFF)
set(COMTRADE_TEST_FONT "" CACHE FILEPATH "Chinese TTF/TTC font for installed AI PDF test")
if(NOT IS_ABSOLUTE "${COMTRADE_JAVA_DIR}" OR NOT EXISTS "${COMTRADE_JAVA_DIR}/comtrade-core-java.jar")
    message(FATAL_ERROR "Set COMTRADE_JAVA_DIR to the absolute installed lib/comtrade/java directory")
endif()
if(COMTRADE_EXPECT_AI AND NOT EXISTS "${COMTRADE_TEST_FONT}")
    message(FATAL_ERROR "Set COMTRADE_TEST_FONT to a Chinese TTF/TTC font when testing AI")
endif()
set(jar "${COMTRADE_JAVA_DIR}/comtrade-core-java.jar")
set(classes "${CMAKE_CURRENT_BINARY_DIR}/classes")
add_custom_command(OUTPUT "${classes}/InstalledConsumer.class"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${classes}"
    COMMAND "${Java_JAVAC_EXECUTABLE}" -encoding UTF-8 -cp "${jar}" -d "${classes}"
        "${CMAKE_CURRENT_SOURCE_DIR}/InstalledConsumer.java"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/InstalledConsumer.java" "${jar}" VERBATIM)
add_custom_target(java_installed_consumer ALL DEPENDS "${classes}/InstalledConsumer.class")
if(WIN32)
    set(classpath "${jar}\;${classes}")
else()
    set(classpath "${jar}:${classes}")
endif()
enable_testing()
add_test(NAME InstalledJava.Consumer COMMAND "${Java_JAVA_EXECUTABLE}"
    "-Djava.library.path=${COMTRADE_JAVA_DIR}" -cp "${classpath}" InstalledConsumer
    "${COMTRADE_JAVA_DIR}" "${COMTRADE_EXPECT_AI}" "${COMTRADE_TEST_FONT}")
set_tests_properties(InstalledJava.Consumer PROPERTIES TIMEOUT 90 LABELS "java;install")

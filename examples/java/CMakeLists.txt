set(java_examples DemoSupport RecordAndCff StreamingAndTail TimeAndRecovery)
set(java_scenarios RecordAndCff StreamingAndTail TimeAndRecovery)
if(COMTRADE_JAVA_AI_ENABLED)
    list(APPEND java_examples AiEvidence)
    list(APPEND java_scenarios AiEvidence)
endif()
set(java_sources)
foreach(name IN LISTS java_examples)
    list(APPEND java_sources "${CMAKE_CURRENT_SOURCE_DIR}/${name}.java")
endforeach()
set(java_output "${CMAKE_CURRENT_BINARY_DIR}/classes")
add_custom_command(OUTPUT "${java_output}/examples.stamp"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${java_output}"
    COMMAND "${Java_JAVAC_EXECUTABLE}" -encoding UTF-8 -cp "${COMTRADE_JAVA_JAR}" -d "${java_output}" ${java_sources}
    COMMAND "${CMAKE_COMMAND}" -E touch "${java_output}/examples.stamp"
    DEPENDS ComtradeCoreJavaJar "${COMTRADE_JAVA_JAR}" ${java_sources}
    VERBATIM)
add_custom_target(ComtradeJavaExamples ALL DEPENDS "${java_output}/examples.stamp")
if(WIN32)
    set(java_classpath "${COMTRADE_JAVA_JAR}\;${java_output}")
else()
    set(java_classpath "${COMTRADE_JAVA_JAR}:${java_output}")
endif()
if(COMTRADE_BUILD_TESTS)
    foreach(name IN LISTS java_scenarios)
        add_test(NAME Examples.Java.${name} COMMAND "${Java_JAVA_EXECUTABLE}"
            "-Djava.library.path=$<TARGET_FILE_DIR:ComtradeCoreJava>" -cp "${java_classpath}" comtrade.examples.${name})
        set_tests_properties(Examples.Java.${name} PROPERTIES LABELS "examples;java" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    endforeach()
endif()

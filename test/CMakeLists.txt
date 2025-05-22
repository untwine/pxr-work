set(_ENV "")

if (WIN32)
    # Patch CMake's 'add_test' to extend the DLL search path on Windows. Use the
    # target name associated with the test, if it is also a CMake target.

    macro(add_test)
        _add_test(${ARGV})

        cmake_parse_arguments("" "" "NAME" "" ${ARGV})
        if (TARGET ${_NAME})
            set(DLL_DIRS $<TARGET_RUNTIME_DLL_DIRS:${_NAME}>)
        else()
            set(DLL_DIRS $<TARGET_RUNTIME_DLL_DIRS:testWorkThreadLimits>)
        endif()

        set(_ENV "PATH=$<JOIN:$<SHELL_PATH:${DLL_DIRS}>,\\;>")
        set_tests_properties(${_NAME} PROPERTIES ENVIRONMENT "${_ENV}")
    endmacro()
endif()


add_executable(testWorkDispatcher testWorkDispatcher.cpp)
target_link_libraries(testWorkDispatcher PUBLIC work)
add_test(NAME testWorkDispatcher COMMAND testWorkDispatcher)

add_executable(testWorkLoops testWorkLoops.cpp)
target_link_libraries(testWorkLoops PUBLIC work)
add_test(NAME testWorkLoops COMMAND testWorkLoops)

add_executable(testWorkReduce testWorkReduce.cpp)
target_link_libraries(testWorkReduce PUBLIC work)
add_test(NAME testWorkReduce COMMAND testWorkReduce)

add_executable(testWorkSort testWorkSort.cpp)
target_link_libraries(testWorkSort PUBLIC work)
add_test(NAME testWorkSort COMMAND testWorkSort)

add_executable(testWorkThreadLimits testWorkThreadLimits.cpp)
target_link_libraries(testWorkThreadLimits PUBLIC work)

add_test(NAME testWorkThreadLimitsDefault COMMAND testWorkThreadLimits)

add_test(NAME testWorkThreadLimits1 COMMAND testWorkThreadLimits)
set_tests_properties(testWorkThreadLimits1
    PROPERTIES ENVIRONMENT "PXR_WORK_THREAD_LIMIT=1;${_ENV}")

add_test(NAME testWorkThreadLimits3 COMMAND testWorkThreadLimits)
set_tests_properties(testWorkThreadLimits3
    PROPERTIES ENVIRONMENT "PXR_WORK_THREAD_LIMIT=3;${_ENV}")

add_test(NAME testWorkThreadLimitsRawTBBMax
    COMMAND testWorkThreadLimits --rawtbb)
set_tests_properties(testWorkThreadLimitsRawTBBMax
    PROPERTIES RUN_SERIAL TRUE)

add_test(NAME testWorkThreadLimitsRawTBB2
    COMMAND testWorkThreadLimits --rawtbb)
set_tests_properties(testWorkThreadLimitsRawTBB2
    PROPERTIES
        RUN_SERIAL TRUE
        ENVIRONMENT "PXR_WORK_THREAD_LIMIT=2;${_ENV}")

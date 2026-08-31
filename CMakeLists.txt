cmake_minimum_required(VERSION 3.16)
project(mcl_wire LANGUAGES C)

option(MCL_WIRE_BUILD_TESTS "Build host-side reference tests" ON)
option(MCL_WIRE_BUILD_BENCHMARKS "Build retained source-size benchmark" ON)

add_library(mcl_wire STATIC
    src/wire.c
    src/extension.c)
target_include_directories(mcl_wire PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(mcl_wire PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO)

if(MCL_WIRE_BUILD_TESTS)
    enable_testing()

    foreach(test_name IN ITEMS wire extension vectors fuzz_decode)
        add_executable(mcl_wire_test_${test_name} tests/test_${test_name}.c)
        target_link_libraries(mcl_wire_test_${test_name} PRIVATE mcl_wire)
        set_target_properties(mcl_wire_test_${test_name} PROPERTIES
            C_STANDARD 99
            C_STANDARD_REQUIRED YES
            C_EXTENSIONS NO)
        add_test(NAME mcl_wire_${test_name} COMMAND mcl_wire_test_${test_name})
    endforeach()
endif()

if(MCL_WIRE_BUILD_BENCHMARKS)
    add_executable(mcl_wire_benchmark_source benchmarks/benchmark_source.c)
    target_link_libraries(mcl_wire_benchmark_source PRIVATE mcl_wire)
    set_target_properties(mcl_wire_benchmark_source PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
endif()

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

    foreach(test_name IN ITEMS wire extension vectors fuzz_decode duration major_rule)
        add_executable(mcl_wire_test_${test_name} tests/test_${test_name}.c)
        target_link_libraries(mcl_wire_test_${test_name} PRIVATE mcl_wire)
        set_target_properties(mcl_wire_test_${test_name} PROPERTIES
            C_STANDARD 99
            C_STANDARD_REQUIRED YES
            C_EXTENSIONS NO)
        add_test(NAME mcl_wire_${test_name} COMMAND mcl_wire_test_${test_name})
    endforeach()

    # Pins the PUBLISHED extension vectors against the decoder. test_extension.c
    # exercises the same behaviour through C arrays, which proves nothing about
    # the JSON an independent implementer would download; the two can drift
    # invisibly.
    add_executable(mcl_wire_validate_extension_vectors
        tools/validate_extension_vectors.c)
    target_link_libraries(mcl_wire_validate_extension_vectors PRIVATE mcl_wire)
    set_target_properties(mcl_wire_validate_extension_vectors PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    add_test(NAME mcl_wire_extension_vectors
        COMMAND mcl_wire_validate_extension_vectors
        "${CMAKE_CURRENT_SOURCE_DIR}/conformance/vectors/extensions-v0.1.json")

    # spec/tier0-layout-v0.2.md is the authoritative statement of the seven
    # Tier-0 body layouts, written so a second implementation can be built
    # without reading this source. A layout document that is WRONG is worse
    # than none: the implementer builds to it, the bytes disagree, and the
    # disagreement gets blamed on whichever side is younger. This transcribes
    # its table as data and checks it against the codec in both directions.
    add_executable(mcl_wire_validate_tier0_layout tools/validate_tier0_layout.c)
    target_link_libraries(mcl_wire_validate_tier0_layout PRIVATE mcl_wire)
    set_target_properties(mcl_wire_validate_tier0_layout PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    add_test(NAME mcl_wire_tier0_layout COMMAND mcl_wire_validate_tier0_layout)

    # The extension mechanism is specified, implemented and reachable from the
    # public API; this checks the registry that allocates for it. The table is
    # empty, and this passing on an empty table is the point: the checks that
    # make the first real assignment safe are in place before the pressure to
    # add one is.
    add_executable(mcl_wire_validate_extension_registry
        tools/validate_extension_registry.c)
    set_target_properties(mcl_wire_validate_extension_registry PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    add_test(NAME mcl_wire_extension_registry
        COMMAND mcl_wire_validate_extension_registry
        "${CMAKE_CURRENT_SOURCE_DIR}/registries/extension-ids-v0.1.json")
endif()

if(MCL_WIRE_BUILD_BENCHMARKS)
    add_executable(mcl_wire_benchmark_source benchmarks/benchmark_source.c)
    target_link_libraries(mcl_wire_benchmark_source PRIVATE mcl_wire)
    set_target_properties(mcl_wire_benchmark_source PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    if(MCL_WIRE_BUILD_TESTS)
        add_test(
            NAME mcl_wire_benchmark_source
            COMMAND mcl_wire_benchmark_source
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/cases-v0.1.json")
    endif()
endif()

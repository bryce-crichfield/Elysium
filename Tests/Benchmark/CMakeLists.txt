# Benchmarks: prints timings, never fails. Only an optimized build gives numbers worth comparing:
# most of what's measured (World's mask lookups, the sol2 bindings) is compiled into Core and
# Services at the build's own level, and a Debug build runs it several times slower. -O2 here
# only covers this target's own code.
file(GLOB_RECURSE BENCHMARK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(ElysiumBenchmarks ${BENCHMARK_SOURCES})

target_link_libraries(ElysiumBenchmarks PRIVATE ElysiumCore ElysiumServices)
# Tests/Unit for the shared fakes (Fakes/*.h).
target_include_directories(ElysiumBenchmarks PRIVATE ${nanobench_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR}/Tests/Unit)
target_compile_options(ElysiumBenchmarks PRIVATE -O2)

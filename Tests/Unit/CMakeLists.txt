# Unit tests. Core and Services only: the editor and games are tested by hand.
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(ElysiumTests ${TEST_SOURCES})

target_link_libraries(ElysiumTests PRIVATE ElysiumCore ElysiumServices doctest::doctest)

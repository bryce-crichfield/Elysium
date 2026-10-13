# The executable: main.cpp and Application, the composition root that creates the services and
# owns the editor.
file(GLOB_RECURSE RUNTIME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(Elysium ${RUNTIME_SOURCES})

target_link_libraries(Elysium PRIVATE
    ElysiumCore
    ElysiumServices
    ElysiumEditor
)

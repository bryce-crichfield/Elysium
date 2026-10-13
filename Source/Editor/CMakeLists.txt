# The in-engine editor. The only layer that uses ImGui.
file(GLOB_RECURSE EDITOR_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(ElysiumEditor OBJECT ${EDITOR_SOURCES})

target_link_libraries(ElysiumEditor
    PUBLIC
        ElysiumCore
        ElysiumServices
    PRIVATE
        imgui
        rlimgui
        colortextedit
        imguizmo
)
target_include_directories(ElysiumEditor PRIVATE
    ${portable_file_dialogs_SOURCE_DIR}
)

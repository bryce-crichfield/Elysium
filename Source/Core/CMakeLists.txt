# The engine core: ECS, scenes, assets, rendering, scripting. Depends on Interfaces/ and vendor code
# only. An OBJECT library rather than a static one: a static library would drop component and
# system files nothing references, and with them their REGISTER_COMPONENT/REGISTER_SYSTEM.
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(ElysiumCore OBJECT ${CORE_SOURCES})

target_link_libraries(ElysiumCore
    PUBLIC
        ElysiumBuild
        raylib
        tinyxml2
        lua
        sol2::sol2
    PRIVATE
        RecastNavigation::Recast
        RecastNavigation::Detour
)
target_include_directories(ElysiumCore PRIVATE
    ${dr_libs_SOURCE_DIR}
    ${stb_SOURCE_DIR}
)

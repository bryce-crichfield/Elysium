cmake_minimum_required(VERSION 3.20)
project(Elysium)
# Editor system refactored

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/Build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Binary)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Binary)

# Find all source files in Source directory
file(GLOB_RECURSE SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_SOURCE_DIR}/Source/*.c"
)

# Add vendor libraries
add_subdirectory(Vendor/raylib)
add_subdirectory(Vendor/tinyxml2)
add_subdirectory(Vendor/SQLiteCpp)
add_subdirectory(Vendor/enet)

# sol2
set(SOL2_TESTS OFF CACHE BOOL "" FORCE)
set(SOL2_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(Vendor/sol2)

# Lua, since it doesn't have a CMakeLists.txt, we have to build it ourselves
set (LUA_DIR ${CMAKE_SOURCE_DIR}/Vendor/lua)
file (GLOB LUA_SOURCES ${LUA_DIR}/*.c)
list (FILTER LUA_SOURCES EXCLUDE REGEX ".*(lua|luac)\\.c$")
add_library (lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC ${LUA_DIR})

# ImGui library, since it doesn't have a CMakeLists.txt, we have to build it ourselves
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/Vendor/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# ImGui Text Editor 
# ImGuiColorTextEdit
set(COLORTEXTEDIT_DIR ${CMAKE_SOURCE_DIR}/Vendor/ImGuiColorTextEdit)
add_library(colortextedit STATIC ${COLORTEXTEDIT_DIR}/TextEditor.cpp)
target_include_directories(colortextedit PUBLIC
    ${COLORTEXTEDIT_DIR}
    ${IMGUI_DIR}
)
target_link_libraries(colortextedit PUBLIC imgui)

# rlImGui library, since it's header only it's a bit more complicated
set(RLIMGUI_DIR ${CMAKE_SOURCE_DIR}/Vendor/rlImGui)
add_library(rlimgui STATIC ${RLIMGUI_DIR}/rlImGui.cpp)
target_include_directories(rlimgui PUBLIC
    ${RLIMGUI_DIR}
    ${CMAKE_SOURCE_DIR}/Vendor/raylib/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(rlimgui PUBLIC raylib imgui)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Tracy profiler
option(TRACY_ENABLE "Enable Tracy profiler" OFF)
if(TRACY_ENABLE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
    set(TRACY_NO_SYSTEM_TRACING ON CACHE BOOL "Disable ETW for MinGW compatibility" FORCE)
    add_subdirectory(Vendor/tracy)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)
    message(STATUS "Tracy profiler enabled")
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME}
    raylib
    rlimgui
    imgui
    colortextedit
    tinyxml2
    SQLiteCpp
    enet
    lua
    sol2::sol2
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    SOL_NO_CHECK_NUMBER_PRECISION=1    
)

# Add Tracy if enabled
if(TRACY_ENABLE)
    target_link_libraries(${PROJECT_NAME} Tracy::TracyClient)
endif()

if(MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE 
        -Wa,-mbig-obj  # Handle large object files
    )
endif()


# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/Source/Core
    ${CMAKE_SOURCE_DIR}/Source/Utilities
    ${CMAKE_SOURCE_DIR}/Source/Services
    ${CMAKE_SOURCE_DIR}/Source/Systems
    ${CMAKE_SOURCE_DIR}/Source/Scenes
    ${CMAKE_SOURCE_DIR}/Source/Network
    ${CMAKE_SOURCE_DIR}/Source/Messages
    ${CMAKE_SOURCE_DIR}/Source/Editor
    ${CMAKE_SOURCE_DIR}/Vendor/raylib/src
    ${CMAKE_SOURCE_DIR}/Vendor/rlImGui
    ${CMAKE_SOURCE_DIR}/Vendor/imgui
    ${CMAKE_SOURCE_DIR}/Vendor/imgui/backends
    ${CMAKE_SOURCE_DIR}/Vendor/tinyxml2
    ${CMAKE_SOURCE_DIR}/Vendor/SQLiteCpp/include
    ${CMAKE_SOURCE_DIR}/Vendor/enet/include
    ${CMAKE_SOURCE_DIR}/Vendor/lua
    ${CMAKE_SOURCE_DIR}/Vendor/sol2/include
    ${CMAKE_SOURCE_DIR}/Vendor/ImGuiColorTextEdit
    ${CMAKE_SOURCE_DIR}/Vendor/tracy/public
)

# Assets always resolve relative to the runtime output directory, in both
# Debug and Release, so there is one asset root regardless of build type.
file(GLOB_RECURSE ASSET_FILES "${CMAKE_SOURCE_DIR}/Assets/*")

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Assets ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets
    DEPENDS ${ASSET_FILES}
    COMMENT "Copying Assets to runtime directory"
)

add_dependencies(${PROJECT_NAME} copy_assets)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ASSETS_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/"
    APPDATA_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AppData/"
)
message(STATUS "Assets will be copied to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/")

# Regenerate SVG-derived prefabs (Content/Svg/*.svg -> Binary/Assets/...) as a
# build step, via Tools/SvgExporter's CLI. Generated prefab XML + textures are
# build artifacts, not source, so they're written straight into the runtime
# Assets directory and never into source-controlled Assets/.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    set(SVG_EXPORT_PYTHON ${Python3_EXECUTABLE})
else()
    # The plain `python`/`python3` executables aren't always on PATH on
    # Windows; fall back to the py.exe launcher, which resolves installs
    # find_package(Python3) may miss.
    find_program(PY_LAUNCHER py)
    if(PY_LAUNCHER)
        set(SVG_EXPORT_PYTHON ${PY_LAUNCHER} -3)
    endif()
endif()

if(SVG_EXPORT_PYTHON)
    set(SVG_EXPORTER_DIR ${CMAKE_SOURCE_DIR}/Tools/SvgExporter)
    file(GLOB SVG_EXPORTER_SOURCES "${SVG_EXPORTER_DIR}/*.py")
    file(GLOB SVG_SOURCES "${CMAKE_SOURCE_DIR}/Content/Svg/*.svg")

    set(SVG_EXPORT_OUTPUTS "")
    foreach(svg ${SVG_SOURCES})
        get_filename_component(svg_name ${svg} NAME_WE)
        set(out_xml "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/Scenes/Prefabs/${svg_name}.xml")
        add_custom_command(
            OUTPUT ${out_xml}
            COMMAND ${SVG_EXPORT_PYTHON} ${SVG_EXPORTER_DIR}/cli.py ${svg}
                    --output-dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets
            DEPENDS ${svg} ${SVG_EXPORTER_SOURCES}
            COMMENT "Exporting SVG prefab: ${svg_name}"
        )
        list(APPEND SVG_EXPORT_OUTPUTS ${out_xml})
    endforeach()

    if(SVG_EXPORT_OUTPUTS)
        add_custom_target(export_svg_assets ALL DEPENDS ${SVG_EXPORT_OUTPUTS})
        add_dependencies(export_svg_assets copy_assets)
        add_dependencies(${PROJECT_NAME} export_svg_assets)
    endif()
else()
    message(WARNING "No Python interpreter found; SVG-derived prefabs under Content/Svg/ will not be regenerated.")
endif()

# Platform-specific settings
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "Elysium"
    )
endif()

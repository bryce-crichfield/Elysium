cmake_minimum_required(VERSION 3.20)
project(Elysium)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build directory for intermediate files
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

# ImGui sources (since ImGui doesn't have CMakeLists.txt by default)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/Vendor/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Create ImGui library
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# rlImGui library (build directly instead of using submodule CMakeLists.txt)
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

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    raylib
    rlimgui
    imgui
    tinyxml2
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Include
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/Vendor/raylib/src
    ${CMAKE_SOURCE_DIR}/Vendor/rlImGui
    ${CMAKE_SOURCE_DIR}/Vendor/imgui
    ${CMAKE_SOURCE_DIR}/Vendor/imgui/backends
    ${CMAKE_SOURCE_DIR}/Vendor/tinyxml2
)

# Track all asset files as dependencies
file(GLOB_RECURSE ASSET_FILES "${CMAKE_SOURCE_DIR}/Assets/*")

# Create custom target for copying assets that depends on asset files
add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Assets ${CMAKE_SOURCE_DIR}/Binary/Assets
    DEPENDS ${ASSET_FILES}
    COMMENT "Copying Assets to Binary directory"
)

# Make executable depend on asset copying
add_dependencies(${PROJECT_NAME} copy_assets)

# Platform-specific settings
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "Elysium.exe"
    )
endif()
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
    tinyxml2
    SQLiteCpp
    enet
)

# Add Tracy if enabled
if(TRACY_ENABLE)
    target_link_libraries(${PROJECT_NAME} Tracy::TracyClient)
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
)

if(TRACY_ENABLE)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/Vendor/tracy/public
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    # In debug, define ASSETS_PATH to point to source directory
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        ASSETS_PATH="${CMAKE_SOURCE_DIR}/Assets/"
    )
    message(STATUS "Debug build: Assets will be loaded from ${CMAKE_SOURCE_DIR}/Assets/")
else()
    # In release, copy assets and define ASSETS_PATH to point to binary directory
    file(GLOB_RECURSE ASSET_FILES "${CMAKE_SOURCE_DIR}/Assets/*")
    
    add_custom_target(copy_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Assets ${CMAKE_SOURCE_DIR}/Binary/Assets
        DEPENDS ${ASSET_FILES}
        COMMENT "Copying Assets to Binary directory"
    )
    
    add_dependencies(${PROJECT_NAME} copy_assets)
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        ASSETS_PATH="Assets/"
    )
    message(STATUS "Release build: Assets will be copied to Binary/Assets/")
endif()

# Platform-specific settings
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "Elysium"
    )
endif()

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

# Add vendor libraries
# raylib's audio module (miniaudio + its own dr_wav/stb_vorbis copies) is off: audio is ours
# (AudioService on PortAudio, decoding in Core/Audio), and the decoders would clash at link time.
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
add_subdirectory(Vendor/raylib)
# glTF models embed JPEG textures; raylib's config.h leaves JPEG off.
target_compile_definitions(raylib PRIVATE SUPPORT_FILEFORMAT_JPG=1)
add_subdirectory(Vendor/tinyxml2)
add_subdirectory(Vendor/SQLiteCpp)
add_subdirectory(Vendor/enet)

# Recast (navmesh baking) and Detour (navmesh queries): NavigationSystem
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)
# It asks for CMake 3.1, which CMake 4 refuses; configure it under 3.5 policies (as PortAudio).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(Vendor/recastnavigation EXCLUDE_FROM_ALL)  # only what we link gets built
unset(CMAKE_POLICY_VERSION_MINIMUM)

# Audio: PortAudio for the device (Windows + Linux), dr_libs + stb_vorbis for decoding files.
# Fetched into Build/_deps on configure, pinned to exact commits.
include(FetchContent)
set(PA_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PA_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PA_USE_ASIO OFF CACHE BOOL "" FORCE)
set(PA_USE_WDMKS OFF CACHE BOOL "" FORCE)            # kernel streaming: needs headers MinGW lacks
set(PA_USE_WDMKS_DEVICE_INFO OFF CACHE BOOL "" FORCE)
FetchContent_Declare(portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_TAG 147dd722548358763a8b649b3e4b41dfffbcfbb6  # v19.7.0
)
FetchContent_Declare(dr_libs
    GIT_REPOSITORY https://github.com/mackron/dr_libs.git
    GIT_TAG dfe8377631000664666519fdb83da193fd8037f4
)
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
)
FetchContent_Declare(portable_file_dialogs
    GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
    GIT_TAG c12ea8c9a727f5320a2b4570aee863bbede2a204
)
# dr_libs and stb have no CMakeLists, so these just download them. PortAudio 19.7.0 asks for
# CMake 3.1, which CMake 4 refuses outright; this lets it configure under 3.5 policies.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(portaudio dr_libs stb portable_file_dialogs)
unset(CMAKE_POLICY_VERSION_MINIMUM)

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

# ImGuizmo: only the transform gizmo is used, so the other widgets aren't built
set(IMGUIZMO_DIR ${CMAKE_SOURCE_DIR}/Vendor/ImGuizmo)
add_library(imguizmo STATIC ${IMGUIZMO_DIR}/src/ImGuizmo.cpp)
target_include_directories(imguizmo PUBLIC ${IMGUIZMO_DIR}/src)
target_link_libraries(imguizmo PUBLIC imgui)

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

# Tracy profiler
option(TRACY_ENABLE "Enable Tracy profiler" OFF)
if(TRACY_ENABLE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
    set(TRACY_NO_SYSTEM_TRACING ON CACHE BOOL "Disable ETW for MinGW compatibility" FORCE)
    add_subdirectory(Vendor/tracy)
    message(STATUS "Tracy profiler enabled")
endif()

if(UNIX AND NOT APPLE AND CMAKE_EXE_LINKER_FLAGS MATCHES "(^| )-static( |$)")
    string(REGEX REPLACE "(^| )-static( |$)" " " _fixed_flags "${CMAKE_EXE_LINKER_FLAGS}")
    string(STRIP "${_fixed_flags}" _fixed_flags)
    set(CMAKE_EXE_LINKER_FLAGS "${_fixed_flags}" CACHE STRING "Linker flags for executables" FORCE)
    message(WARNING "Removed -static from CMAKE_EXE_LINKER_FLAGS (unsupported with OpenGL/ALSA/JACK on Linux)")
endif()

# Settings every Elysium target shares. Includes are written from Source/ ("Core/Scene.h"),
# and Common.h's profiler macros need Tracy's headers whether or not it's enabled.
add_library(ElysiumBuild INTERFACE)
target_include_directories(ElysiumBuild INTERFACE
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/Vendor/tracy/public
)
target_compile_definitions(ElysiumBuild INTERFACE
    SOL_NO_CHECK_NUMBER_PRECISION=1
    ASSETS_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/"
)
if(TRACY_ENABLE)
    target_compile_definitions(ElysiumBuild INTERFACE TRACY_ENABLE)
    target_link_libraries(ElysiumBuild INTERFACE Tracy::TracyClient)
endif()
if(MINGW)
    target_compile_options(ElysiumBuild INTERFACE -Wa,-mbig-obj)  # Handle large object files
endif()

# The engine's layers, each with its own vendor dependencies. Core reaches services only through
# Interfaces/; Services and Editor build on Core; Runtime is the executable that wires them up.
add_subdirectory(Source/Core)
add_subdirectory(Source/Services)
add_subdirectory(Source/Editor)
add_subdirectory(Source/Runtime)

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
message(STATUS "Assets will be copied to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/")

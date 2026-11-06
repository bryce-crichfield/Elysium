# The services behind Interfaces/: assets, audio, scenes, scripting, networking, logging.
# Web/ holds the browser's stand-ins; they build everywhere so the unit tests cover them, while
# the desktop audio (PortAudio) and network (ENet) services don't build for the browser at all.
file(GLOB_RECURSE SERVICES_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if(EMSCRIPTEN)
    list(REMOVE_ITEM SERVICES_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/AudioService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkService.cpp
    )
endif()
add_library(ElysiumServices OBJECT ${SERVICES_SOURCES})

target_link_libraries(ElysiumServices PUBLIC ElysiumCore)
if(NOT EMSCRIPTEN)
    target_link_libraries(ElysiumServices PRIVATE enet portaudio_static)
    target_include_directories(ElysiumServices PRIVATE
        ${CMAKE_SOURCE_DIR}/Vendor/enet/include
        ${portaudio_SOURCE_DIR}/include
    )
endif()

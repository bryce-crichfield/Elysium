# The services behind Interfaces/: assets, audio, scenes, scripting, networking, logging.
file(GLOB_RECURSE SERVICES_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(ElysiumServices OBJECT ${SERVICES_SOURCES})

target_link_libraries(ElysiumServices
    PUBLIC
        ElysiumCore
    PRIVATE
        enet
        portaudio_static
)
target_include_directories(ElysiumServices PRIVATE
    ${CMAKE_SOURCE_DIR}/Vendor/enet/include
    ${portaudio_SOURCE_DIR}/include
)

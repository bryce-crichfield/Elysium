# The executable: a main per platform, which picks the services (and on desktop, the editor),
# and Application, which runs them.
file(GLOB_RECURSE RUNTIME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

if(EMSCRIPTEN)
    list(REMOVE_ITEM RUNTIME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/DesktopMain.cpp)
    add_executable(Elysium ${RUNTIME_SOURCES})
    target_link_libraries(Elysium PRIVATE ElysiumCore ElysiumServices)

    # The one project the page runs. It and the engine's Assets are packed into Elysium.data and
    # mounted in the page's in-memory filesystem, so the engine's file reads work unchanged.
    set(ELYSIUM_WEB_PROJECT "Projects/Battler" CACHE STRING "Project directory the Web build runs")
    target_compile_definitions(Elysium PRIVATE
        ELYSIUM_WEB_PROJECT="/${ELYSIUM_WEB_PROJECT}/Project.xml"
    )
    set_target_properties(Elysium PROPERTIES
        SUFFIX ".html"
        LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Web/Shell.html
    )
    target_link_options(Elysium PRIVATE
        -fexceptions
        -sUSE_GLFW=3
        -sMIN_WEBGL_VERSION=2
        -sMAX_WEBGL_VERSION=2
        -sGL_ENABLE_GET_PROC_ADDRESS=1
        -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=5MB
        -sFORCE_FILESYSTEM=1
        # The page: the canvas fills the tab, and fullscreen is the browser's (F11), which raylib
        # sees as a resize. Emscripten's own fullscreen resizes the canvas behind raylib's back.
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/Web/Shell.html"
        "--preload-file=${CMAKE_SOURCE_DIR}/Assets@/Assets"
        "--preload-file=${CMAKE_SOURCE_DIR}/${ELYSIUM_WEB_PROJECT}@/${ELYSIUM_WEB_PROJECT}"
    )
else()
    list(REMOVE_ITEM RUNTIME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/WebMain.cpp)
    add_executable(Elysium ${RUNTIME_SOURCES})
    target_link_libraries(Elysium PRIVATE ElysiumCore ElysiumServices ElysiumEditor)
endif()

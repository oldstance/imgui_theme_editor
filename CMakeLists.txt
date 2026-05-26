cmake_minimum_required(VERSION 3.24)

project(imgui_theme_editor
    VERSION 0.1.0
    DESCRIPTION "Interactive Dear ImGui style/theme editor with JSON persistence and C++ export"
    LANGUAGES CXX C)

# ---------------------------------------------------------------------------
# Global compile options
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive- /Zc:__cplusplus)
    add_compile_options(/wd4100 /wd4189 /wd4244 /wd4267 /wd4127 /wd4505)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX UNICODE _UNICODE)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---------------------------------------------------------------------------
# Third party dependencies (fetched at configure time)
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# nlohmann/json (header only)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# GLFW (windowing/input)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Dear ImGui (no top-level CMakeLists; we build only what we need)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.4
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _do_not_descend_)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)
if(WIN32)
    target_link_libraries(imgui PUBLIC opengl32)
endif()
if(MSVC)
    target_compile_options(imgui PRIVATE /wd4505 /wd4127 /wd4244 /wd4267)
endif()

# ---------------------------------------------------------------------------
# imgui_theme_editor executable
# ---------------------------------------------------------------------------
set(THEME_EDITOR_SOURCES
    imgui_theme_editor/sources/main.cpp
    imgui_theme_editor/sources/style_serializer.cpp
    imgui_theme_editor/sources/theme_editor.cpp
)

set(THEME_EDITOR_HEADERS
    imgui_theme_editor/sources/style_serializer.h
    imgui_theme_editor/sources/theme_editor.h
)

add_executable(imgui_theme_editor
    ${THEME_EDITOR_SOURCES}
    ${THEME_EDITOR_HEADERS})

target_include_directories(imgui_theme_editor PRIVATE
    imgui_theme_editor/sources)

target_link_libraries(imgui_theme_editor PRIVATE
    imgui
    glfw
    nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(imgui_theme_editor PRIVATE opengl32)
endif()

source_group("Headers" FILES ${THEME_EDITOR_HEADERS})
source_group("Sources" FILES ${THEME_EDITOR_SOURCES})

if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY VS_STARTUP_PROJECT imgui_theme_editor)
endif()

install(TARGETS imgui_theme_editor RUNTIME DESTINATION bin)

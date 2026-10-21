cmake_minimum_required(VERSION 3.16)
project(MicroSMPlay LANGUAGES CXX)

# Keep local development builds on the debuggable configuration by default.
if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "Available build configurations" FORCE)
else()
    set(CMAKE_DEFAULT_BUILD_TYPE Debug CACHE STRING "Default build configuration")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9
    GIT_SHALLOW TRUE
)

set(SDL2_DISABLE_SDL2MAIN OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_TESTS ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.0
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG v1.1.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL2)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

FetchContent_MakeAvailable(nfd)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2)

add_executable(microsmplay
    src/main.cpp
    src/ChartParser.cpp
    src/TimingData.cpp
    src/AudioEngine.cpp
    src/GameState.cpp
    src/Renderer.cpp
    src/MenuSystem.cpp
    src/SongBrowser.cpp
    src/MinaCalcAdapter.cpp
    src/MinaCalc/MinaCalc.cpp
    src/miniaudio_impl.cpp
    src/stb_vorbis_impl.cpp
)

target_include_directories(microsmplay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_definitions(microsmplay PRIVATE STANDALONE_CALC)

target_link_libraries(microsmplay PRIVATE SDL2::SDL2 SDL2::SDL2main imgui nfd)

if(MSVC)
    target_compile_options(microsmplay PRIVATE /W4)
else()
    target_compile_options(microsmplay PRIVATE -Wall -Wextra -Wpedantic)
endif()

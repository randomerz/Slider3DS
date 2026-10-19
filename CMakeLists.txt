# PC build (SDL2 backend). The 3DS build uses the devkitPro Makefile instead.
cmake_minimum_required(VERSION 3.20)
project(slider3ds CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL2)

# SDL2_image with only its built-in (stb) PNG loader.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
foreach(fmt AVIF BMP GIF JPG JXL LBM PCX PNM QOI SVG TGA TIF WEBP XCF XPM XV)
    set(SDL2IMAGE_${fmt} OFF CACHE BOOL "" FORCE)
endforeach()
set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2_image
    URL https://github.com/libsdl-org/SDL_image/releases/download/release-2.8.2/SDL2_image-2.8.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL2_image)

# Platform-free game code, shared by the game and the tests.
# One folder level, like the 3DS Makefile (src/game/<system>/*.cpp).
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS src/game/*/*.cpp)
add_library(slider3ds_game STATIC ${GAME_SOURCES})
target_include_directories(slider3ds_game PUBLIC src)

# Ogg Vorbis decoding for the PC build and the tests (src/platform/stb, with asset files read from
# memory; the 3DS uses libvorbisidec).
add_library(stb_vorbis STATIC third_party/stb_vorbis.c)
target_include_directories(stb_vorbis PUBLIC third_party)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(slider3ds ${APP_SOURCES} src/platform/sdl/platform_sdl.cpp src/platform/stb/ogg_stb.cpp src/platform/stb/asset_mem.cpp)
option(SLIDER_DEV "Debug controls and overlay in the PC build" ON)
target_compile_definitions(slider3ds PRIVATE ASSET_ROOT="${CMAKE_SOURCE_DIR}/assets" SLIDER_DEV=$<BOOL:${SLIDER_DEV}>)
target_link_libraries(slider3ds PRIVATE slider3ds_game stb_vorbis SDL2::SDL2main SDL2::SDL2-static SDL2_image::SDL2_image-static)

# Headless tests against the exported data (tests/test_platform.cpp stands in for a backend).
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(slider3ds_tests ${TEST_SOURCES} src/platform/stb/ogg_stb.cpp src/platform/stb/asset_mem.cpp)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test-out)
target_compile_definitions(slider3ds_tests PRIVATE ASSET_ROOT="${CMAKE_SOURCE_DIR}/assets"
    TESTS_DIR="${CMAKE_SOURCE_DIR}/tests" TEST_OUT_DIR="${CMAKE_BINARY_DIR}/test-out")
target_link_libraries(slider3ds_tests PRIVATE slider3ds_game stb_vorbis)

# Strict warnings on our own targets only (not SDL).
foreach(t slider3ds_game slider3ds slider3ds_tests)
    if(MSVC)
        target_compile_options(${t} PRIVATE /W4 /permissive- /D_CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${t} PRIVATE -Wall -Wextra)
    endif()
endforeach()

enable_testing()
add_test(NAME slider3ds_tests COMMAND slider3ds_tests)

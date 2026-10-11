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

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp src/game/*.cpp)

add_executable(slider3ds ${GAME_SOURCES} src/platform/sdl/platform_sdl.cpp)
target_include_directories(slider3ds PRIVATE src)
target_compile_definitions(slider3ds PRIVATE ASSET_ROOT="${CMAKE_SOURCE_DIR}/assets")
target_link_libraries(slider3ds PRIVATE SDL2::SDL2main SDL2::SDL2-static SDL2_image::SDL2_image-static)

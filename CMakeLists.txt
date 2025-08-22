cmake_minimum_required(VERSION 3.16)

project(Scratch-Everywhere VERSION 0.19 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
    message(STATUS "Downloading CPM.cmake...")
    file(DOWNLOAD "https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake"
         "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake"
         STATUS status LOG log_content)
    if(status)
        message(FATAL_ERROR "Failed to download CPM.cmake: ${status}\n${log_content}")
    endif()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

CPMAddPackage(
    NAME SDL2
    GITHUB_REPOSITORY libsdl-org/SDL
	GIT_TAG release-2.32.8
    VERSION 2.32.8
)
CPMAddPackage(
    NAME SDL2_image
    GITHUB_REPOSITORY libsdl-org/SDL_image
	GIT_TAG release-2.8.8
    VERSION 2.8.8
)
CPMAddPackage(
    NAME SDL2_mixer
    GITHUB_REPOSITORY libsdl-org/SDL_mixer
	GIT_TAG release-2.8.1
    VERSION 2.8.1
)
CPMAddPackage(
    NAME SDL2_ttf
    GITHUB_REPOSITORY libsdl-org/SDL_ttf
	GIT_TAG release-2.24.0
    VERSION 2.24.0
)

CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.12.0
)

include(FetchContent)
FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
)
FetchContent_MakeAvailable(miniz)

# TODO: add cloud variable support

set(SOURCES source source/scratch source/scratch/blocks source/scratch/menus source/sdl source/sdl/audio)
set(SOURCE_FILES)
foreach(DIR IN LISTS SOURCES)
    file(GLOB DIR_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.c
         ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cc
    )
	list(APPEND SOURCE_FILES ${DIR_SOURCES})
endforeach()
list(REMOVE_DUPLICATES SOURCE_FILES)

list(APPEND SOURCE_FILES "${miniz_SOURCE_DIR}/miniz.c")

add_executable(scratch-everywhere ${SOURCE_FILES})

target_compile_definitions(scratch-everywhere PRIVATE __PC__ SDL_BUILD ENABLE_AUDIO) # TODO: make these configurable

target_link_libraries(scratch-everywhere PRIVATE
	SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    SDL2_ttf::SDL2_ttf
    nlohmann_json::nlohmann_json
)

target_include_directories(scratch-everywhere PRIVATE ${SOURCES} ${miniz_SOURCE_DIR})

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

if(NINTENDO_3DS)
CPMAddPackage(
    NAME SDL2
    GITHUB_REPOSITORY libsdl-org/SDL
	GIT_TAG release-2.32.8
    VERSION 2.32.8
)
CPMAddPackage(
    NAME SDL2_mixer
    GITHUB_REPOSITORY libsdl-org/SDL_mixer
	GIT_TAG release-2.8.1
    VERSION 2.8.1
	OPTIONS "SDL2MIXER_VORBIS STB" "SDL2MIXER_MP3 MINIMP3" "SDL2MIXER_FLAC DRFLAC" "SDL2MIXER_MOD OFF" "SDL2MIXER_OPUS OFF" "SDL2MIXER_FLUIDSYNTH OFF" "SDL2MIXER_WAVPACK OFF" "SDL2MIXER_MIDI OFF"
)
CPMAddPackage(
	NAME nanosvg
	GITHUB_REPOSITORY memononen/nanosvg
	GIT_TAG HEAD # TODO: pin commit
)
CPMAddPackage(
    NAME stb
    GITHUB_REPOSITORY nothings/stb
	GIT_TAG master # TODO: pin commit
	DOWNLOAD_ONLY YES
)
else()
find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image>=2.0.0)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf>=2.0.0)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer>=2.0.0)
endif()

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

if(NINTENDO_3DS)
set(SOURCES source source/scratch source/scratch/blocks source/scratch/menus source/3ds source/sdl/audio)
else()
set(SOURCES source source/scratch source/scratch/blocks source/scratch/menus source/sdl source/sdl/audio)
endif()

set(SOURCE_FILES)
foreach(DIR IN LISTS SOURCES)
    file(GLOB DIR_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.c
         ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cc
    )
	list(APPEND SOURCE_FILES ${DIR_SOURCES})
endforeach()

list(APPEND SOURCE_FILES "${miniz_SOURCE_DIR}/miniz.c")

add_executable(scratch-everywhere ${SOURCE_FILES})

if (NOT NINTENDO_3DS)
	target_include_directories(scratch-everywhere PRIVATE
		${SDL2_INCLUDE_DIRS}
	    ${SDL2_IMAGE_INCLUDE_DIRS}
	    ${SDL2_TTF_INCLUDE_DIRS}
	    ${SDL2_MIXER_INCLUDE_DIRS}
	)

	target_link_libraries(scratch-everywhere PRIVATE
	    ${SDL2_LIBRARIES}
		${SDL2_IMAGE_LIBRARIES}
	    ${SDL2_TTF_LIBRARIES}
		${SDL2_MIXER_LIBRARIES}
	)
endif()

# TODO: add cloud variable support

if(NINTENDO_WIIU OR NINTENDO_SWITCH)
	target_compile_definitions(scratch-everywhere PRIVATE SDL_BUILD ENABLE_AUDIO)
elseif(NINTENDO_3DS)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_AUDIO)
else()
	target_compile_definitions(scratch-everywhere PRIVATE __PC__ SDL_BUILD ENABLE_AUDIO) # TODO: make these configurable
endif()

target_link_libraries(scratch-everywhere PRIVATE
    nlohmann_json::nlohmann_json
)


if(NINTENDO_3DS)
	target_link_libraries(scratch-everywhere PRIVATE SDL2::SDL2 SDL2_mixer::SDL2_mixer-static nanosvg citro2d citro3d)
endif()

if(NINTENDO_3DS)
	target_include_directories(scratch-everywhere PRIVATE ${SOURCES} ${miniz_SOURCE_DIR} "${nanosvg_SOURCE_DIR}/src" ${stb_SOURCE_DIR})
else()
	target_include_directories(scratch-everywhere PRIVATE ${SOURCES} ${miniz_SOURCE_DIR})
endif()

set(VITA_APP_NAME "Scratch Everywhere!")
set(VITA_TITLEID "NTXS00053")

file(GLOB GFXFILES "${CMAKE_SOURCE_DIR}/gfx/*")

if (NINTENDO_WIIU)
	wut_create_rpx(scratch-everywhere)
elseif(NINTENDO_3DS)
	ctr_create_3dsx(scratch-everywhere ROMFS ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
elseif(NINTENDO_SWITCH)
	nx_create_nro(scratch-everywhere ROMFS ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
elseif(NINTENDO_WII OR NINTENDO_GAMECUBE)
	ogc_create_dol(scratch-everywhere)
elseif(VITA)
	include("${VITASDK}/share/vita.cmake" REQUIRED)

	set(VITA_GFXFILES)
	foreach(file IN LISTS GFXFILES)
		if(IS_DIRECTORY "${file}")
			continue()
		endif()

		list(APPEND VITA_GFXFILES FILE "${file}" "${file}")
	endforeach()

	vita_create_self(scratch-everywhere.self scratch-everywhere)
	vita_create_vpk(scratch-everywhere.vpk ${VITA_TITLEID} scratch-everywhere.self
		VERSION "01.00"
		NAME ${VITA_APP_NAME}
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/icon0.png sce_sys/icon0.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
		${VITA_GFXFILES}
	)
endif()

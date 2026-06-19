cmake_minimum_required(VERSION 3.2)
project(YSFLIGHT)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsStore" AND NOT ANDROID AND NOT EMSCRIPTEN)
	set(BUILD_YSFLIGHT_CONSOLE_SERVER 1)
endif()



if("${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsStore")
	add_definitions(-DYS_IS_UNIVERSAL_WINDOWS_APP)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /ZW")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /ZW")
endif()



include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Web build is single-threaded on purpose.  With -pthread (and no
# -sPROXY_TO_PTHREAD) main() and the rAF loop run on the browser main thread,
# and YsThreadPool::Run blocks it on a condition_variable while worker threads
# run the parallel sim tasks.  Blocking the browser main thread freezes rAF and
# the whole page whenever a worker is slow/stalled (heavy collision step, shared
# memory growth, GC, scheduling) -- the reported hang.  Without -pthread the
# __EMSCRIPTEN_PTHREADS__ inline fallbacks in ysclass11 (YsThreadPool,
# YsRunTaskParallel) run tasks inline on the main thread instead: never faster
# than native, but it never wedges the page.  Re-enabling threads requires
# moving the loop off the main thread (-sPROXY_TO_PTHREAD + OffscreenCanvas).
# (No -pthread also means SharedArrayBuffer / COOP+COEP are no longer required.)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../public/src)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../public/src public)
else()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../public/src public)
endif()

add_subdirectory(common)
add_subdirectory(config)
add_subdirectory(platform/common)
add_subdirectory(filename)
add_subdirectory(dynamics)
add_subdirectory(autopilot)
add_subdirectory(pathplanning)
add_subdirectory(externalconsole)
add_subdirectory(util)
add_subdirectory(vehicle)
add_subdirectory(scenery)
add_subdirectory(ysjoystick/src)
set(ysflight_filename_INCLUDE_DIR ${ysflight_filename_INCLUDE_DIR} PARENT_SCOPE)

add_subdirectory(graphics/common)
add_subdirectory(graphics/gl)
add_subdirectory(graphics/gl2.0)
add_subdirectory(graphics/null)

if(ysd3d9_EXIST)
	add_subdirectory(graphics/d3d9)
endif()

if(MSVC)
	add_subdirectory(platform/win32)
	add_subdirectory(sounddll/win32)
	add_subdirectory(voicedll/generic)
elseif(ANDROID)
	add_subdirectory(platform/android)
elseif(APPLE)
	add_subdirectory(platform/macosx)
	add_subdirectory(sounddll/macosx)
	add_subdirectory(voicedll/generic)
elseif(EMSCRIPTEN)
	add_subdirectory(${YSFLIGHT_WEB_PORT_DIR}/platform_emscripten platform_emscripten)
elseif(UNIX)
	add_subdirectory(platform/linux)
	add_subdirectory(sounddll/linux-alsa)
	add_subdirectory(sounddll/linux-pulseaudio)
	add_subdirectory(voicedll/generic)
else()
	add_subdirectory(voicedll/generic)
endif()


if(BUILD_YSFLIGHT_CONSOLE_SERVER)
	add_subdirectory(platform/nownd)
endif()


add_subdirectory(core)
add_subdirectory(ui)
add_subdirectory(gui)

add_subdirectory(main)
if(NOT EMSCRIPTEN)
	add_subdirectory(main_consvr)
	add_subdirectory(main_demoOnly)
	if(BUILD_YSFLIGHT_CONSOLE_SERVER)
		add_subdirectory(main_harness)
	endif()
endif()

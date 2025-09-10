cmake_minimum_required(VERSION 3.10)
# cmake_policy(SET CMP0167 OLD)

project(av-tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set(BUILD_SHARED_LIBS ON)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
	libavcodec
	libavformat
	libavutil
	libswresample
)

file(GLOB_RECURSE SOURCES
	${PROJECT_NAME}/capi/*.cpp
	${PROJECT_NAME}/ffmpeg/*.cpp
	${PROJECT_NAME}/utils/*.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_SOURCE_DIR}
	${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME} PRIVATE
	${FFMPEG_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	Boost::boost
	OpenSSL::SSL
	OpenSSL::Crypto
	${FFMPEG_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME avtools)

install(
	TARGETS ${PROJECT_NAME}
	LIBRARY
	DESTINATION lib
)

install(
	DIRECTORY ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/capi
	DIRECTORY ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/ffmpeg
	DIRECTORY ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/utils
	DESTINATION include/${PROJECT_NAME}
	FILES_MATCHING
		PATTERN "*.h"
		PATTERN "*.hpp"
)

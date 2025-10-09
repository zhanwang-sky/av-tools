cmake_minimum_required(VERSION 3.10)
# cmake_policy(SET CMP0167 OLD)

project(av-tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RTMPDUMP_PATH "/opt/rtmpdump" CACHE PATH "Path to custom RTMPDump installation")
set(FFMPEG_PATH "/opt/ffmpeg" CACHE PATH "Path to custom FFmpeg installation")

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set(BUILD_SHARED_LIBS ON)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

file(GLOB_RECURSE SOURCES
	${PROJECT_NAME}/capi/*.cpp
	${PROJECT_NAME}/ffmpeg/*.cpp
	${PROJECT_NAME}/utils/*.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_SOURCE_DIR}
	${RTMPDUMP_PATH}/include
	${FFMPEG_PATH}/include
)

target_link_directories(${PROJECT_NAME} PRIVATE
	${RTMPDUMP_PATH}/lib
	${FFMPEG_PATH}/lib
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	Boost::boost
	OpenSSL::SSL
	OpenSSL::Crypto
	rtmp
	avcodec avformat avutil swresample
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

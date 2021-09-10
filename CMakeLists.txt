# vi: set noexpandtab syntax=cmake:

PROJECT(ffmpeg_process)
INCLUDE(GNUInstallDirs)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")

# add_definitions(-fexceptions -finstrument-functions -funwind-tables -g -rdynamic -O0)

if (DEFINED DRAW_BY_DRM)
	add_definitions(-DDRAW_BY_DRM=${DRAW_BY_DRM})
endif()

set(RK_IMAGE_PROCESS_LINK_LIBRARIES avutil avformat avcodec swscale)

set(RK_IMAGE_PROCESS_SRC image_process.cpp)

add_executable(ffmpeg_process ${RK_IMAGE_PROCESS_SRC})

target_include_directories(ffmpeg_process
                           PUBLIC ${GFLAGS_INCLUDE_DIRS}
                           PUBLIC ${LIBAVUTIL_INCLUDE_DIRS}
                           PUBLIC ${LIBDRM_INCLUDE_DIRS}
                          )

target_link_libraries(ffmpeg_process ${RK_IMAGE_PROCESS_LINK_LIBRARIES})

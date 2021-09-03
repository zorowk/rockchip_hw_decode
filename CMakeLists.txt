cmake_minimum_required(VERSION 2.8)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(/usr/include/drm )
include_directories(/usr/include/glib-2.0 )

set(link_libs dl rga pthread drm opencv_core opencv_highgui opencv_imgcodecs opencv_imgproc)
add_executable(rga_v4l2 rga_v4l2.cpp rga_const.cpp)
target_link_libraries(rga_v4l2 ${link_libs})
target_compile_definitions(rga_v4l2 PUBLIC LINUX)

INCLUDE(GNUInstallDirs)

INSTALL(TARGETS rga_v4l2 RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

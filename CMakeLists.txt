cmake_minimum_required(VERSION 3.10)
project(RTSPPlayer)

set(CMAKE_CXX_STANDARD 17)

# GLFW
set(GLFW_DIR "D:/study/rtsp-player/3rdparty/glfw")
include_directories(${GLFW_DIR}/include)
link_directories(${GLFW_DIR}/lib-vc2022)

# OpenGL 라이브러리 찾기
find_package(OpenGL REQUIRED)

add_executable(RTSPPlayer main.cpp)

target_link_libraries(RTSPPlayer
    OpenGL::GL
    glfw3
)
cmake_minimum_required(VERSION 3.10)
project(RTSPPlayer)

set(CMAKE_CXX_STANDARD 17)

# GLFW
set(GLFW_DIR "D:/study/rtsp-player/3rdparty/glfw")
include_directories(${GLFW_DIR}/include)
link_directories(${GLFW_DIR}/lib-vc2022)

# GLAD
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/third-party/glad)
# Glad 소스 추가
include_directories(${GLAD_DIR}/include)
file(GLOB GLAD_SOURCES ${GLAD_DIR}/src/*.c)

# stb_image
set(STB_DIR ${CMAKE_SOURCE_DIR}/third-party/stb)
include_directories(${STB_DIR})

# OpenGL 라이브러리 찾기
find_package(OpenGL REQUIRED)

add_executable(RTSPPlayer main.cpp ${GLAD_SOURCES})

target_link_libraries(RTSPPlayer
    OpenGL::GL
    glfw3
)
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

# FFmpeg 경로 설정
set(FFMPEG_DIR ${CMAKE_SOURCE_DIR}/third-party/ffmpeg)

include_directories(${FFMPEG_DIR}/include)
link_directories(${FFMPEG_DIR}/lib)

# DLL이 있는 디렉토리 설정
set(FFMPEG_DLL_DIR "${FFMPEG_DIR}/bin" CACHE PATH "FFmpeg DLL directory")

# 필요한 DLL 목록
set(FFMPEG_DLLS
    avcodec-61.dll
    avformat-61.dll
    avutil-59.dll
    swscale-8.dll
    swresample-5.dll
)


add_executable(RTSPPlayer 
    FrameQueue.h
    decode-thread.h
    debug-message-handler.h
    check-error.h
    open-rtsp.h
    create-window.h
    init-gl.h
    print-info.h
    vertex-buffer.h
    render-scene.h
    Renderer.cpp
    Renderer.h    
    main.cpp 
    ${GLAD_SOURCES})

target_link_libraries(RTSPPlayer
    OpenGL::GL
    glfw3
    avcodec 
    avformat 
    avutil 
    swscale
)

if(MSVC)
    target_link_options(RTSPPlayer PUBLIC "-NODEFAULTLIB:libcmt") # 또는 "-NODEFAULTLIB:msvcrt"
endif()

# DLL 복사 함수 정의
function(copy_dlls TARGET_NAME DLL_LIST DLL_SOURCE_DIR)
    foreach(DLL ${DLL_LIST})
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL_SOURCE_DIR}/${DLL}"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/${DLL}"
            COMMENT "Copying ${DLL} to output directory"
        )
    endforeach()
endfunction()

# 빌드 후 DLL 복사 실행
copy_dlls(RTSPPlayer "${FFMPEG_DLLS}" "${FFMPEG_DLL_DIR}")

# # 개발 중에는 작업 디렉토리를 출력 디렉토리로 설정
# set_target_properties(RTSPPlayer PROPERTIES
#     VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:RTSPPlayer>"
# )
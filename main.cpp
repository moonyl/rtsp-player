#include <iostream>
#include <glad/glad.h> // Glad를 포함합니다.
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <chrono>
#include <iostream>
#include <thread>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h> 
    #include <libavutil/time.h>
} 

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord; // 텍스처 좌표 추가
out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord; // 텍스처 좌표 전송
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 fragColor;
in vec2 TexCoord;
uniform sampler2D texture1; // 텍스처 샘플러

void main() {
    fragColor = texture(texture1, vec2(TexCoord.x, 1.0 - TexCoord.y)); // 영상 상하 반전
}
)"; 

void GLAPIENTRY MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    // 메시지 종류에 따른 접두사 설정
    const char* severityStr;
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        severityStr = "HIGH";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        severityStr = "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        severityStr = "LOW";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        severityStr = "NOTIFICATION";
        break;
    default:
        severityStr = "UNKNOWN";
    }

    // 메시지 타입 구분
    const char* typeStr;
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        typeStr = "ERROR";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        typeStr = "DEPRECATED";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        typeStr = "UNDEFINED";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        typeStr = "PORTABILITY";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        typeStr = "PERFORMANCE";
        break;
    case GL_DEBUG_TYPE_OTHER:
        typeStr = "OTHER";
        break;
    default:
        typeStr = "UNKNOWN";
    }

    // 소스 구분
    const char* sourceStr;
    switch (source) {
    case GL_DEBUG_SOURCE_API:
        sourceStr = "API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        sourceStr = "WINDOW SYSTEM";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        sourceStr = "SHADER COMPILER";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        sourceStr = "THIRD PARTY";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        sourceStr = "APPLICATION";
        break;
    case GL_DEBUG_SOURCE_OTHER:
        sourceStr = "OTHER";
        break;
    default:
        sourceStr = "UNKNOWN";
    }

    // 메시지 출력
    std::cerr << "GL " << severityStr << " [" << typeStr << "] from " << sourceStr
        << " (ID: " << id << "): " << message << std::endl;

    // 높은 심각도의 에러인 경우 프로그램 중단
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        std::cerr << "Aborting due to high severity message" << std::endl;
        abort();
    }
}

int main() {

    // FFmpeg 초기화
    AVFormatContext* formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, "../../CAM3_edit_rd.mkv", nullptr, nullptr) != 0) {
        std::cerr << "Could not open video file" << std::endl;
        return -1;
    }
    
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Could not find stream info" << std::endl;
        return -1;
    }

    // 비디오 스트림 찾기
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "Could not find video stream" << std::endl;
        return -1;
    }

    // 코덱 설정
    AVCodecParameters* codecParams = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, codecParams);
    avcodec_open2(codecContext, codec, nullptr);

    // 프레임과 패킷 할당
    AVFrame* frame = av_frame_alloc();
    AVFrame* frameRGB = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();

    // RGB 변환을 위한 버퍼 설정
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width,
                                          codecContext->height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer,
                        AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);

    // SwsContext 설정
    SwsContext* swsContext = sws_getContext(
        codecContext->width, codecContext->height, codecContext->pix_fmt,
        codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    // GLFW 초기화
    if (!glfwInit()) {
        const char* description;
        int code = glfwGetError(&description);
        std::cerr << "GLFW 초기화 실패: " << description << " (코드: " << code << ")" << std::endl;

        return -1;
    }

    // OpenGL 버전과 프로파일 설정
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);    
    glfwWindowHint(GLFW_DOUBLEBUFFER, true);    

    // GLFW 윈도우 생성
    GLFWwindow* window = glfwCreateWindow(800, 600, "RTSP Player", NULL, NULL);
    if (!window) {
        const char* description;
        int code = glfwGetError(&description);
        std::cerr << "GLFW 윈도우 생성 실패: " << description << " (코드: " << code << ")" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);  

    // VSync를 비활성화
    glfwSwapInterval(0);  // VSync 비활성화

    // Glad 초기화
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Glad 초기화 실패" << std::endl;
        return -1;
    }

    // 6. 디버그 출력 설정
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);


    const GLubyte* renderer = glGetString(GL_RENDERER); // GPU 이름
    const GLubyte* version = glGetString(GL_VERSION);   // OpenGL 버전

    std::cout << "Renderer: " << renderer << std::endl;
    std::cout << "OpenGL Version: " << version << std::endl;

        // VBO 및 VAO 설정
    float vertices[] = {
        // positions                    // texture coords
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f, // top right
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f, // bottom right
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, // bottom left
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f  // top left 
    };

    unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 위치 속성
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // 텍스처 좌표 속성
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);


    // 버텍스 셰이더 컴파일
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    // 셰이더 컴파일 오류 확인
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "버텍스 셰이더 컴파일 실패: " << infoLog << std::endl;
    }

    // 프래그먼트 셰이더 컴파일
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    // 셰이더 컴파일 오류 확인
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "프래그먼트 셰이더 컴파일 실패: " << infoLog << std::endl;
    }

    // 셰이더 프로그램 링크
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    //glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0); // 텍스처 유닛 0과 연결

    // 셰이더 삭제
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);    

    // 텍스처 로드
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, codecContext->width, codecContext->height,
            0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    // 텍스처의 필라(필터링) 옵션 설정
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // // 이미지 로드
    // int width, height, nrChannels;
    
    // stbi_set_flip_vertically_on_load(true);
    // // unsigned char *data = stbi_load("../../jetpack-check.png", &width, &height, &nrChannels, 0); // 이미지 파일 경로
    // unsigned char *data = stbi_load("../../jetpack-check.jpg", &width, &height, &nrChannels, 0); // 이미지 파일 경로
    // // unsigned char *data = stbi_load("../../container.jpg", &width, &height, &nrChannels, 0); // 이미지 파일 경로
    // if (data) {
    //     // 텍스처 생성
    //     if (nrChannels == 4)    {
    //         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    //     } else {
    //         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    //     }
        
        
    //     glGenerateMipmap(GL_TEXTURE_2D);
    // } else {
    //     std::cerr << "텍스처 로드 실패" << std::endl;
    // }
    // stbi_image_free(data);

    // 비디오 타이밍을 위한 변수들
    double fps = av_q2d(formatContext->streams[videoStreamIndex]->r_frame_rate);
    double frame_delay = 1.0 / fps;
    int64_t start_time = av_gettime();
    int64_t frame_pts = 0;
    
    // 타임베이스 계산
    AVRational time_base = formatContext->streams[videoStreamIndex]->time_base;
    double time_base_double = av_q2d(time_base);

    // 메인 루프
    while (!glfwWindowShouldClose(window)) {

        // // 함수 실행 전후에 시간을 측정하는 예제
        // static auto start = std::chrono::high_resolution_clock::now();

        // auto end = std::chrono::high_resolution_clock::now();
        // //std::chrono::duration<double> duration = end - start;
        // //std::cout << "av_read_frame took " << duration.count() << " seconds." << std::endl;
        // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        // std::cout << "av_read_frame took " << duration.count() << " msec." << std::endl;

        // start = end;

        int64_t current_time = av_gettime() - start_time;

        // 프레임 읽기
        if (av_read_frame(formatContext, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex) {
                // 패킷 디코딩
                avcodec_send_packet(codecContext, packet);
                if (avcodec_receive_frame(codecContext, frame) == 0) {
                    // 프레임의 표시 시간 계산
                    frame_pts = static_cast<int64_t>(frame->pts * time_base_double * 1000000);
                    
                    // 프레임이 표시될 시간이 되었는지 확인
                    if (frame_pts > current_time) {
                        // 다음 프레임까지 대기
                        int64_t sleep_time = frame_pts - current_time;
                        if (sleep_time > 0 && sleep_time < 1000000) {  // 1초 이하의 대기만 허용
                            std::this_thread::sleep_for(std::chrono::microseconds(sleep_time));
                        }
                    }

                    // RGB로 변환
                    sws_scale(swsContext, frame->data, frame->linesize, 0,
                            codecContext->height, frameRGB->data, frameRGB->linesize);

                    // OpenGL 텍스처 업데이트
                    glBindTexture(GL_TEXTURE_2D, texture);
                    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, codecContext->width, codecContext->height,
                    //             0, GL_RGB, GL_UNSIGNED_BYTE, frameRGB->data[0]);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, codecContext->width, codecContext->height, GL_RGB, GL_UNSIGNED_BYTE, frameRGB->data[0]);
                }
                else {
                    glfwPollEvents();
                    continue;
                }
            }
            av_packet_unref(packet);
        } else {
            // 파일의 끝에 도달하면 처음으로 되감기
            av_seek_frame(formatContext, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
        }

        // 화면을 지우기
        glClear(GL_COLOR_BUFFER_BIT);

        // 셰이더 프로그램 사용
        glUseProgram(shaderProgram);

        // 텍스처 사용
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        

        // VAO 바인딩 및 삼각형 그리기
        glBindVertexArray(VAO);
        // glDrawArrays(GL_TRIANGLES, 0, 3); // 삼각형 그리기
        // glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0); // 인덱스 드로우
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); // 인덱스 드로우

        // 윈도우 버퍼 교환
        glfwSwapBuffers(window);

        glfwPollEvents();

    }

    // 정리
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    av_packet_free(&packet);
    av_free(buffer);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);
    sws_freeContext(swsContext);

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window); 
    glfwTerminate();

    return 0;
}
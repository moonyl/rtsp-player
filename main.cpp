#include <iostream>
#include <glad/glad.h> // Glad를 포함합니다.
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <chrono>
#include <iostream>
#include <thread>

#include <queue>
#include <mutex>
#include <condition_variable>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h> 
    #include <libavutil/time.h>
    #include <libavutil/hwcontext.h>
    #include <libavutil/hwcontext_dxva2.h>
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


// 프레임 버퍼 구조체
struct FrameBuffer {
    uint8_t* data;
    int size;
    int64_t pts;
};

// 전역 변수
std::queue<FrameBuffer> frameQueue;
std::mutex queueMutex;
std::condition_variable queueCondition;
bool isPlaying = true;
const int MAX_QUEUE_SIZE = 30;

// 하드웨어 디코딩을 위한 컨텍스트
static AVBufferRef* hw_device_ctx = nullptr;

static int init_hw_decoder(AVCodecContext* ctx) {
    int err = 0;
    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_DXVA2, nullptr, nullptr, 0)) < 0) {
        std::cerr << "Failed to create DXVA2 device: " << err << std::endl;
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    return err;
}

// 디코딩 스레드 함수
void decodingThread(AVFormatContext* formatContext, int videoStreamIndex, 
                   AVCodecContext* codecContext,
                   int width, int height) {
    AVFrame* frame = av_frame_alloc();
    AVFrame* sw_frame = av_frame_alloc();
    AVFrame* frameRGB = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    
    // RGB 버퍼 설정
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer,
                        AV_PIX_FMT_RGB24, width, height, 1);

    // 하드웨어 디코딩된 프레임을 RGB로 변환하기 위한 SwsContext
    SwsContext* swsContext = sws_getContext(
        width, height, AV_PIX_FMT_NV12,  // DXVA2 디코더의 일반적인 출력 포맷
        width, height, AV_PIX_FMT_RGB24, // OpenGL 텍스처용 포맷
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );                        

    while (isPlaying) {
        // 함수 실행 전후에 시간을 측정하는 예제
        // auto start = std::chrono::high_resolution_clock::now();     

        if (av_read_frame(formatContext, packet) >= 0) {
        // auto end = std::chrono::high_resolution_clock::now();
        //std::chrono::duration<double> duration = end - start;
        //std::cout << "av_read_frame took " << duration.count() << " seconds." << std::endl;
        // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        // std::cout << "av_read_frame took " << duration.count() << " msec." << std::endl;   

            if (packet->stream_index == videoStreamIndex) {
                int ret = avcodec_send_packet(codecContext, packet);
                if (ret < 0) {
                    std::cerr << "Error sending packet for decoding" << std::endl;
                    continue;
                }                
                
                while (ret >= 0) {
                    ret = avcodec_receive_frame(codecContext, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        std::cerr << "Error during decoding" << std::endl;
                        break;
                    }

                    // 하드웨어 프레임을 시스템 메모리로 전송
                    if (frame->format == AV_PIX_FMT_DXVA2_VLD) {
                        if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) {
                            std::cerr << "Error transferring frame from GPU to CPU" << std::endl;
                            continue;
                        }
                        
                        // NV12에서 RGB로 변환
                        sws_scale(swsContext, sw_frame->data, sw_frame->linesize, 0,
                                height, frameRGB->data, frameRGB->linesize);
                    } else {
                        // 하드웨어 가속이 실패한 경우 직접 변환
                        sws_scale(swsContext, frame->data, frame->linesize, 0,
                                height, frameRGB->data, frameRGB->linesize);
                    }                                        

                    // 프레임 큐에 추가
                    std::unique_lock<std::mutex> lock(queueMutex);
                    queueCondition.wait(lock, [] { return frameQueue.size() < MAX_QUEUE_SIZE; });

                    FrameBuffer fb;
                    fb.size = numBytes;
                    fb.data = (uint8_t*)av_malloc(numBytes);
                    memcpy(fb.data, frameRGB->data[0], numBytes);
                    fb.pts = frame->pts;

                    frameQueue.push(fb);
                    lock.unlock();
                    queueCondition.notify_one();
                }
                
            }
            av_packet_unref(packet);
        } else {
            // RTSP의 경우 스트림이 끝나도 계속 시도
            av_seek_frame(formatContext, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
        }
    }
    sws_freeContext(swsContext);
    av_frame_free(&frame);
    av_frame_free(&sw_frame);
    av_frame_free(&frameRGB);
    av_packet_free(&packet);
    av_free(buffer);
}

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
    // FFmpeg 네트워king 초기화
    avformat_network_init();

    // RTSP 연결 설정
    AVDictionary* options = nullptr;
    // RTSP 연결 타임아웃 설정 (5초)
    av_dict_set(&options, "rtsp_transport", "tcp", 0);  // TCP 사용
    av_dict_set(&options, "stimeout", "5000000", 0);   // 타임아웃 5초
    // FFmpeg 초기화
    AVFormatContext* formatContext = avformat_alloc_context();

    // RTSP URL 설정 (실제 RTSP 스트림 URL로 변경 필요)
    const char* rtspUrl = "rtsp://admin:q1w2e3r4@@192.168.15.83:50554/rtsp/camera1/high";

    if (avformat_open_input(&formatContext, rtspUrl, nullptr, &options) != 0) {
        std::cerr << "Could not open RTSP stream" << std::endl;
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

    // 하드웨어 가속 디코더 설정
    AVCodecParameters* codecParams = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder_by_name("h264_dxva2"); // DXVA2 디코더
    if (!codec) {
        std::cerr << "DXVA2 decoder not found, falling back to CPU decoder" << std::endl;
        codec = avcodec_find_decoder(codecParams->codec_id);
    }

    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, codecParams);

    // 하드웨어 디코더 초기화
    if (init_hw_decoder(codecContext) < 0) {
        std::cerr << "Failed to initialize hardware decoder, falling back to software decoding" << std::endl;
        codec = avcodec_find_decoder(codecParams->codec_id);
        codecContext = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecContext, codecParams);
    }

    // 디코더 픽셀 포맷 설정
    codecContext->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) -> enum AVPixelFormat {
        const enum AVPixelFormat* p;
        for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
            if (*p == AV_PIX_FMT_DXVA2_VLD)
                return *p;
        }
        std::cerr << "Failed to get DXVA2 format, falling back to default" << std::endl;
        return pix_fmts[0];
    };

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        return -1;
    }

    // // 코덱 설정
    // AVCodecParameters* codecParams = formatContext->streams[videoStreamIndex]->codecpar;
    // const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    // AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    // avcodec_parameters_to_context(codecContext, codecParams);
    // avcodec_open2(codecContext, codec, nullptr);


    // // SwsContext 설정
    // SwsContext* swsContext = sws_getContext(
    //     codecContext->width, codecContext->height, codecContext->pix_fmt,
    //     codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
    //     SWS_BILINEAR, nullptr, nullptr, nullptr
    // );

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



    // 비디오 타이밍을 위한 변수들
    double fps = av_q2d(formatContext->streams[videoStreamIndex]->r_frame_rate);
    double frame_delay = 1.0 / fps;
    int64_t start_time = av_gettime();
    int64_t frame_pts = 0;
    
    // 타임베이스 계산
    AVRational time_base = formatContext->streams[videoStreamIndex]->time_base;
    double time_base_double = av_q2d(time_base);

    // 디코딩 스레드 시작
    std::thread decoder(decodingThread, formatContext, videoStreamIndex, 
                       codecContext, codecContext->width, codecContext->height);
    // 메인 루프
    while (!glfwWindowShouldClose(window)) {

        // 프레임 가져오기
        FrameBuffer currentFrame;
        bool hasFrame = false;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (!frameQueue.empty()) {
                currentFrame = frameQueue.front();
                frameQueue.pop();
                hasFrame = true;
                queueCondition.notify_one();
            }
        }

        if (hasFrame) {
            // OpenGL 텍스처 업데이트
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                           codecContext->width, codecContext->height,
                           GL_RGB, GL_UNSIGNED_BYTE, currentFrame.data);

            av_free(currentFrame.data);
        }

        // OpenGL 렌더링 (기존 코드와 동일)
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();



    }

    // 정리
    isPlaying = false;
    decoder.join();

    if (hw_device_ctx)
        av_buffer_unref(&hw_device_ctx);

    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);
    // sws_freeContext(swsContext);

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window); 
    glfwTerminate();

    return 0;
}
#include <iostream>
#include <glad/glad.h> // Glad를 포함합니다.
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
    fragColor = texture(texture1, TexCoord); // 텍스처에서 색상 샘플링
}
)";


int main() {
    // GLFW 초기화
    if (!glfwInit()) {
        const char* description;
        int code = glfwGetError(&description);
        std::cerr << "GLFW 초기화 실패: " << description << " (코드: " << code << ")" << std::endl;

        return -1;
    }

    // OpenGL 버전과 프로파일 설정
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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

    // Glad 초기화
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Glad 초기화 실패" << std::endl;
        return -1;
    }

    // // VBO 및 VAO 설정
    // float vertices[] = {
    //     // 위치          // 텍스처 좌표
    //     -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
    //      0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
    //      0.0f,  0.5f, 0.0f,  0.5f, 1.0f,
    // };

        // VBO 및 VAO 설정
    float vertices[] = {
        // positions                    // texture coords
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f, // top right
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, // bottom left
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f  // top left 
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

    // 셰이더 삭제
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0); // 텍스처 유닛 0과 연결

    // 텍스처 로드
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // 텍스처의 필라(필터링) 옵션 설정
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 이미지 로드
    int width, height, nrChannels;
    
    stbi_set_flip_vertically_on_load(true);
    // unsigned char *data = stbi_load("../../jetpack-check.png", &width, &height, &nrChannels, 0); // 이미지 파일 경로
    unsigned char *data = stbi_load("../../jetpack-check.jpg", &width, &height, &nrChannels, 0); // 이미지 파일 경로
    // unsigned char *data = stbi_load("../../container.jpg", &width, &height, &nrChannels, 0); // 이미지 파일 경로
    if (data) {
        // 텍스처 생성
        if (nrChannels == 4)    {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        }
        
        
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "텍스처 로드 실패" << std::endl;
    }
    stbi_image_free(data);

    // 메인 루프
    while (!glfwWindowShouldClose(window)) {
        // 화면을 지우기
        glClear(GL_COLOR_BUFFER_BIT);
        
        // 텍스처 사용
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        // 셰이더 프로그램 사용
        glUseProgram(shaderProgram);
        

        // VAO 바인딩 및 삼각형 그리기
        glBindVertexArray(VAO);
        // glDrawArrays(GL_TRIANGLES, 0, 3); // 삼각형 그리기
        // glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0); // 인덱스 드로우
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); // 인덱스 드로우

        // 윈도우 버퍼 교환
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window); 
    glfwTerminate();

    return 0;
}
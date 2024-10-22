#include <iostream>
#include <glad/glad.h> // Glad를 포함합니다.
#include <GLFW/glfw3.h>

// 셰이더 소스 코드 (버텍스 셰이더와 프래그먼트 셰이더)
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    void main() {
        gl_Position = vec4(aPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    void main() {
        FragColor = vec4(1.0, 0.5, 0.2, 1.0); // 주황색
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


    // 삼각형 정점 데이터
    float vertices[] = {
        // 위치
         0.0f,  0.5f, 0.0f,   // 정점 1 (위)
        -0.5f, -0.5f, 0.0f,   // 정점 2 (왼쪽 아래)
         0.5f, -0.5f, 0.0f    // 정점 3 (오른쪽 아래)
    };

    // VAO, VBO 생성
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    // VBO 바인딩
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // VAO 바인딩
    glBindVertexArray(VAO);

    // 정점 속성 설정
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // VAO와 VBO 언바인딩
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 메인 루프
    while (!glfwWindowShouldClose(window)) {
        // 화면을 지우기
        glClear(GL_COLOR_BUFFER_BIT);

        // 여기에서 OpenGL 렌더링 작업을 수행할 수 있습니다.
        // 셰이더 프로그램 사용
        glUseProgram(shaderProgram);

        // VAO 바인딩 및 삼각형 그리기
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3); // 삼각형 그리기

        // 윈도우 버퍼 교환
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window); 
    glfwTerminate();

    return 0;
}
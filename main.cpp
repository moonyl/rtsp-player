#include <iostream>
#include <GLFW/glfw3.h>

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

    // 메인 루프
    while (!glfwWindowShouldClose(window)) {
        // 화면을 지우기
        glClear(GL_COLOR_BUFFER_BIT);

        // 여기에서 OpenGL 렌더링 작업을 수행할 수 있습니다.

        // 윈도우 버퍼 교환
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window); 
    glfwTerminate();

    return 0;
}
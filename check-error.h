#pragma once
// 에러 발생 시 예외를 던지는 함수
void check_ffmpeg_error(int ret, const std::string &message) {
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    throw std::runtime_error(message + ": " + errbuf);
  }
}

// 에러 처리를 위한 함수
bool checkGLCall(const char *func, const char *file, int line) {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "OpenGL Error (" << err << "): " << func << " in " << file
              << ":" << line << std::endl;
    return false;
  }
  return true;
}

// 매크로를 사용하여 OpenGL 함수 호출 후 에러 체크
#define GL_CALL(x)                                                             \
  do {                                                                         \
    x;                                                                         \
    if (!checkGLCall(#x, __FILE__, __LINE__))                                  \
      return false;                                                            \
  } while (0)

// Helper function to check for shader compilation/linking errors
bool checkShaderCompilation(unsigned int shader,
                            const std::string &shaderType) {
  int success;
  char infoLog[512];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    std::cerr << shaderType << " 컴파일 실패: " << infoLog << std::endl;
    return false;
  }
  return true;
}

bool checkProgramLinking(unsigned int program) {
  int success;
  char infoLog[512];
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(program, 512, nullptr, infoLog);
    std::cerr << "셰이더 프로그램 링크 실패: " << infoLog << std::endl;
    return false;
  }
  return true;
}
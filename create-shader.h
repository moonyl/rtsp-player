#pragma once

unsigned int createShaderProgram(const char *vertexShaderSource,
                                 const char *fragmentShaderSource) {
  // 버텍스 셰이더 컴파일
  unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
  glCompileShader(vertexShader);
  if (!checkShaderCompilation(vertexShader, "버텍스 셰이더")) {
    glDeleteShader(vertexShader);
    return 0; // 에러 발생시 0 반환
  }

  // 프래그먼트 셰이더 컴파일
  unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
  glCompileShader(fragmentShader);
  if (!checkShaderCompilation(fragmentShader, "프래그먼트 셰이더")) {
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return 0; // 에러 발생시 0 반환
  }

  // 셰이더 프로그램 링크
  unsigned int shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  if (!checkProgramLinking(shaderProgram)) {
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteProgram(shaderProgram);
    return 0; // 에러 발생시 0 반환
  }

  // 셰이더 삭제 (링크 후 삭제)
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return shaderProgram;
}
#pragma once

#include "check-error.h"
#include "glad/glad.h"

struct Buffers {
  unsigned int VAO;
  unsigned int VBO;
  unsigned int EBO;
  Buffers() : VAO(0), VBO(0), EBO(0) {}
  ~Buffers() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
  }
};

// VBO, VAO, EBO 설정 함수 (수정)
inline std::unique_ptr<Buffers> setupVAO() {
  auto buffers = std::make_unique<Buffers>(); // unique_ptr 생성
  float vertices[] = {1.0f, 1.0f,  0.0f, 1.0f,  1.0f,  1.0f, -1.0f,
                      0.0f, 1.0f,  0.0f, -1.0f, -1.0f, 0.0f, 0.0f,
                      0.0f, -1.0f, 1.0f, 0.0f,  0.0f,  1.0f};
  unsigned int indices[] = {0, 1, 3, 1, 2, 3};

  GL_CALL(glGenVertexArrays(1, &buffers->VAO));
  GL_CALL(glGenBuffers(1, &buffers->VBO));
  GL_CALL(glGenBuffers(1, &buffers->EBO));

  GL_CALL(glBindVertexArray(buffers->VAO));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffers->VBO));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                       GL_STATIC_DRAW));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers->EBO));
  GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                       GL_STATIC_DRAW));
  GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                                (void *)0));
  GL_CALL(glEnableVertexAttribArray(0));
  GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                                (void *)(3 * sizeof(float))));
  GL_CALL(glEnableVertexAttribArray(1));

  return buffers; // unique_ptr 반환
}

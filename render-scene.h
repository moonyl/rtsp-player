#pragma once

void updateTexture(GLuint texture, const FrameBuffer &frame, int width,
                   int height) {
  if (frame.data == nullptr) {
    std::cerr << "Error: Frame data is null!" << std::endl;
    return;
  }
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB,
                  GL_UNSIGNED_BYTE, frame.data);
  av_free(frame.data);
}

void renderScene(GLuint shaderProgram, GLuint texture, GLuint VAO) {
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(shaderProgram);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

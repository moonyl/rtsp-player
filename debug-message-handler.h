#pragma once

#include <map>
// 심각도, 타입, 소스를 문자열로 변환하는 헬퍼 함수
const std::string getSeverityString(GLenum severity) {
  static const std::map<GLenum, std::string> severityMap = {
      {GL_DEBUG_SEVERITY_HIGH, "HIGH"},
      {GL_DEBUG_SEVERITY_MEDIUM, "MEDIUM"},
      {GL_DEBUG_SEVERITY_LOW, "LOW"},
      {GL_DEBUG_SEVERITY_NOTIFICATION, "NOTIFICATION"},
  };
  auto it = severityMap.find(severity);
  return it != severityMap.end() ? it->second : "UNKNOWN";
}

const std::string getTypeString(GLenum type) {
  static const std::map<GLenum, std::string> typeMap = {
      {GL_DEBUG_TYPE_ERROR, "ERROR"},
      {GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "DEPRECATED"},
      {GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, "UNDEFINED"},
      {GL_DEBUG_TYPE_PORTABILITY, "PORTABILITY"},
      {GL_DEBUG_TYPE_PERFORMANCE, "PERFORMANCE"},
      {GL_DEBUG_TYPE_OTHER, "OTHER"},
  };
  auto it = typeMap.find(type);
  return it != typeMap.end() ? it->second : "UNKNOWN";
}

const std::string getSourceString(GLenum source) {
  static const std::map<GLenum, std::string> sourceMap = {
      {GL_DEBUG_SOURCE_API, "API"},
      {GL_DEBUG_SOURCE_WINDOW_SYSTEM, "WINDOW SYSTEM"},
      {GL_DEBUG_SOURCE_SHADER_COMPILER, "SHADER COMPILER"},
      {GL_DEBUG_SOURCE_THIRD_PARTY, "THIRD PARTY"},
      {GL_DEBUG_SOURCE_APPLICATION, "APPLICATION"},
      {GL_DEBUG_SOURCE_OTHER, "OTHER"},
  };
  auto it = sourceMap.find(source);
  return it != sourceMap.end() ? it->second : "UNKNOWN";
}

void handleDebugMessage(GLenum severity, const std::string &type,
                        const std::string &source, GLuint id,
                        const std::string &message) {
  std::cerr << "GL " << getSeverityString(severity) << " [" << type << "] from "
            << source << " (ID: " << id << "): " << message << std::endl;

  if (severity == GL_DEBUG_SEVERITY_HIGH) {
    std::cerr << "Aborting due to high severity message" << std::endl;
    abort();
  }
}

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length,
                                const GLchar *message, const void *userParam) {
  handleDebugMessage(severity, getTypeString(type), getSourceString(source), id,
                     message);
}
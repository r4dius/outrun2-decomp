#pragma once
#define GL_GLEXT_PROTOTYPES 1
#include <GL/glcorearb.h>
#include <EGL/egl.h>
#include <string>
namespace outrun::ps5_runtime {
unsigned gl_program(const std::string& vertex,const std::string& fragment);
void gl_require(const char* operation);
}

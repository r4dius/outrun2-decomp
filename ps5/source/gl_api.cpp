#include "gl_api.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
namespace outrun::ps5_runtime {
void gl_require(const char* operation){const auto error=glGetError();if(error!=GL_NO_ERROR)
    throw std::runtime_error(std::string(operation)+": GL error "+std::to_string(error));}
unsigned gl_program(const std::string& vertex,const std::string& fragment){
    auto compile=[](GLenum stage,const std::string& text){
        const auto shader=glCreateShader(stage);const char* data=text.c_str();glShaderSource(shader,1,&data,nullptr);glCompileShader(shader);
        GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        if(!ok){GLint n=0;glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&n);std::vector<char> log(std::max(1,n));glGetShaderInfoLog(shader,n,nullptr,log.data());
            glDeleteShader(shader);throw std::runtime_error(std::string("PS5 GLSL compile: ")+log.data());}
        return shader;
    };
    const auto vs=compile(GL_VERTEX_SHADER,vertex);unsigned fs=0,program=0;
    try{fs=compile(GL_FRAGMENT_SHADER,fragment);program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
        GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);
        if(!ok){GLint n=0;glGetProgramiv(program,GL_INFO_LOG_LENGTH,&n);std::vector<char> log(std::max(1,n));glGetProgramInfoLog(program,n,nullptr,log.data());
            throw std::runtime_error(std::string("PS5 GLSL link: ")+log.data());}
        glDeleteShader(vs);glDeleteShader(fs);gl_require("program link");return program;
    }catch(...){glDeleteShader(vs);if(fs)glDeleteShader(fs);if(program)glDeleteProgram(program);throw;}
}
}

#pragma once
#include "RHIShader.h"
#include "glad/glad.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace minEngine
{
    class OpenGLShader : public RHIShader
    {
    public:
        // program ID
        unsigned int m_ID;

        // constructor
        OpenGLShader(const char* vertexShaderPath, const char* fragmentShaderPath);

        // use program
        void Use();

        // uniform tool functions
        void SetBool(const std::string &name, bool value) const;
        void SetInt(const std::string &name, int value) const;
        void SetFloat(const std::string &name, float value) const;
    };
}
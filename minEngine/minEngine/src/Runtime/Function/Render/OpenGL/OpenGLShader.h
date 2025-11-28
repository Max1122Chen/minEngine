#pragma once
#include "Runtime/Function/Render/RHIShader.h"
#include "Runtime/Core/Math/Math.h"
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
        void UploadUniformInt(const std::string& name, int value);
        void UploadUniformFloat(const std::string& name, float value);
        void UploadUniformFloat3(const std::string& name, Vector3 value);
        void UploadUniformMat4(const std::string& name, const float* matrix);
    };
}
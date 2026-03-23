#pragma once
#include "Runtime/Function/Render/RHI/RHIShader.h"
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

        OpenGLShader(const char* vertexShaderPath, const char* fragmentShaderPath);
        virtual ~OpenGLShader() override {} // TODO: add glDeleteProgram(m_ID);

        // use program
        virtual void Use() override;

        // uniform tool functions
        virtual void UploadUniformInt(const std::string& name, int value) override;
        virtual void UploadUniformFloat(const std::string& name, float value) override;
        virtual void UploadUniformFloat3(const std::string& name, Vector3 value) override;
        virtual void UploadUniformMat4(const std::string& name, const float* matrix) override;
        virtual void UploadUniformMat4(const std::string& name, const Matrix4& matrix) override;

    private:
        bool IsValidUniform(const std::string& name, int& uniformLocation);
    };
}
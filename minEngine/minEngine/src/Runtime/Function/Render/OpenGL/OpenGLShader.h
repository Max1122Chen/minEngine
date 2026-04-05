#pragma once
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Core/Math/Math.h"
#include "glad/glad.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

namespace minEngine
{
    class OpenGLShader : public RHIShader
    {
    public:
        // program ID
        unsigned int m_ID;

        OpenGLShader(const char* vertexShaderPath, const char* fragmentShaderPath);
        virtual ~OpenGLShader() override;

        OpenGLShader(const OpenGLShader&) = delete;
        OpenGLShader& operator=(const OpenGLShader&) = delete;

        // use program
        virtual void Use() override;

        // uniform tool functions
        virtual void UploadUniformInt(const std::string& name, int value) override;
        virtual void UploadUniformFloat(const std::string& name, float value) override;
        virtual void UploadUniformFloat3(const std::string& name, Vector3 value) override;
        virtual void UploadUniformMat4(const std::string& name, const float* matrix) override;
        virtual void UploadUniformMat4(const std::string& name, const Matrix4& matrix) override;

        virtual void BindUniformBlock(const std::string& blockName, uint32_t bindingPoint) override;

    private:
        std::unordered_map<std::string, int> m_UniformLocationCache;
        std::unordered_map<std::string, int> m_UniformBlockIndexCache;

        bool IsValidUniform(const std::string& name, int& uniformLocation);
        bool IsValidUniformBlock(const std::string& blockName, int& blockIndex);
    };
}
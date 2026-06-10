#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "glad/glad.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace minEngine
{
    class OpenGLShader
    {
    public:
        unsigned int m_ID = 0;

        OpenGLShader(std::string_view vertexSource, std::string_view fragmentSource);
        ~OpenGLShader();

        OpenGLShader(const OpenGLShader&) = delete;
        OpenGLShader& operator=(const OpenGLShader&) = delete;

        bool IsValid() const { return m_IsValid; }
        const std::string& GetCompileLog() const { return m_CompileLog; }

        void Use();

    private:
        bool m_IsValid = false;
        std::string m_CompileLog;
        std::unordered_map<std::string, int> m_UniformLocationCache;
        std::unordered_map<std::string, int> m_UniformBlockIndexCache;

        bool IsValidUniform(const std::string& name, int& uniformLocation);
        bool IsValidUniformBlock(const std::string& blockName, int& blockIndex);
    };
}

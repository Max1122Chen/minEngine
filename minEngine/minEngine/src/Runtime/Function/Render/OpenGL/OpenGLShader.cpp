#include "OpenGLShader.h"

#include "Log/LogSystem.h"

#include <vector>

namespace minEngine
{
    namespace
    {
        std::string ReadShaderInfoLog(unsigned int shaderObject)
        {
            int logLength = 0;
            glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength <= 1)
            {
                return {};
            }

            std::vector<char> logBuffer(static_cast<size_t>(logLength));
            glGetShaderInfoLog(shaderObject, logLength, nullptr, logBuffer.data());
            return std::string(logBuffer.data());
        }

        std::string ReadProgramInfoLog(unsigned int programObject)
        {
            int logLength = 0;
            glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength <= 1)
            {
                return {};
            }

            std::vector<char> logBuffer(static_cast<size_t>(logLength));
            glGetProgramInfoLog(programObject, logLength, nullptr, logBuffer.data());
            return std::string(logBuffer.data());
        }

        bool CompileShaderStage(unsigned int shaderObject, GLenum stage, std::string_view source, std::string& outLog)
        {
            const char* sourcePtr = source.data();
            const int sourceLength = static_cast<int>(source.size());
            glShaderSource(shaderObject, 1, &sourcePtr, &sourceLength);
            glCompileShader(shaderObject);

            int compileStatus = GL_FALSE;
            glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &compileStatus);
            if (compileStatus == GL_TRUE)
            {
                return true;
            }

            outLog = ReadShaderInfoLog(shaderObject);
            if (outLog.empty())
            {
                outLog = stage == GL_VERTEX_SHADER ? "Vertex shader compilation failed." : "Fragment shader compilation failed.";
            }
            return false;
        }
    }

    OpenGLShader::~OpenGLShader()
    {
        if (m_ID != 0)
        {
            glDeleteProgram(m_ID);
            m_ID = 0;
        }
    }

    OpenGLShader::OpenGLShader(std::string_view vertexSource, std::string_view fragmentSource)
    {
        const unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        const unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        std::string stageLog;
        if (!CompileShaderStage(vertexShader, GL_VERTEX_SHADER, vertexSource, stageLog))
        {
            m_CompileLog = "Vertex shader compile error:\n" + stageLog;
            ME_CORE_ERROR("{}", m_CompileLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return;
        }

        if (!CompileShaderStage(fragmentShader, GL_FRAGMENT_SHADER, fragmentSource, stageLog))
        {
            m_CompileLog = "Fragment shader compile error:\n" + stageLog;
            ME_CORE_ERROR("{}", m_CompileLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return;
        }

        m_ID = glCreateProgram();
        glAttachShader(m_ID, vertexShader);
        glAttachShader(m_ID, fragmentShader);
        glLinkProgram(m_ID);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        int linkStatus = GL_FALSE;
        glGetProgramiv(m_ID, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE)
        {
            m_CompileLog = "Shader program link error:\n" + ReadProgramInfoLog(m_ID);
            ME_CORE_ERROR("{}", m_CompileLog);
            glDeleteProgram(m_ID);
            m_ID = 0;
            return;
        }

        m_IsValid = true;
    }

    void OpenGLShader::Use()
    {
        if (!m_IsValid)
        {
            return;
        }

        glUseProgram(m_ID);
    }

    void OpenGLShader::UploadUniformInt(const std::string &name, int value)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniform1i(uniformLocation, value);
        }
    }

    void OpenGLShader::UploadUniformFloat(const std::string &name, float value)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniform1f(uniformLocation, value);
        }
    }

    void OpenGLShader::UploadUniformFloat2(const std::string &name, const Vector2& value)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniform2f(uniformLocation, value.x, value.y);
        }
    }

    void OpenGLShader::UploadUniformFloat3(const std::string &name, const Vector3& value)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniform3f(uniformLocation, value.x,  value.y, value.z);
        }

    }

    void OpenGLShader::UploadUniformFloat4(const std::string &name, const Vector4& value)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniform4f(uniformLocation, value.x, value.y, value.z, value.w);
        }
    }

    void OpenGLShader::UploadUniformMat4(const std::string &name, const float *matrix)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, matrix);
        }
    }

    void OpenGLShader::UploadUniformMat4(const std::string &name, const Matrix4 &matrix)
    {
        const float* matPtr = glm::value_ptr(matrix);
        UploadUniformMat4(name, matPtr);
    }

    void OpenGLShader::BindUniformBlock(const std::string &blockName, uint32_t bindingPoint)
    {
        int blockIndex = -1;
        if(IsValidUniformBlock(blockName, blockIndex))
        {
            glUniformBlockBinding(m_ID, blockIndex, bindingPoint);
        }
    }

    bool OpenGLShader::IsValidUniform(const std::string &name, int &uniformLocation)
    {
        if (!m_IsValid)
        {
            return false;
        }

        auto cacheIt = m_UniformLocationCache.find(name);
        if (cacheIt != m_UniformLocationCache.end())
        {
            uniformLocation = cacheIt->second;
        }
        else
        {
            uniformLocation = glGetUniformLocation(m_ID, name.c_str());
            m_UniformLocationCache.emplace(name, uniformLocation);
        }

        if(uniformLocation == -1)
        {
            return false;
        }
        return true;
    }

    bool OpenGLShader::IsValidUniformBlock(const std::string &blockName, int &blockIndex)
    {
        if (!m_IsValid)
        {
            return false;
        }

        auto cacheIt = m_UniformBlockIndexCache.find(blockName);
        if (cacheIt != m_UniformBlockIndexCache.end())
        {
            blockIndex = cacheIt->second;
        }
        else
        {
            blockIndex = static_cast<int>(glGetUniformBlockIndex(m_ID, blockName.c_str()));
            m_UniformBlockIndexCache.emplace(blockName, blockIndex);
        }

        if(blockIndex == GL_INVALID_INDEX)
        {
            return false;
        }
        return true;
    }
}

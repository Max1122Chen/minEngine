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
}

#include "Shader.h"

#include "Log/LogSystem.h"
#include "OpenGL/OpenGLShader.h"
#include "RHI/RHI.h"

#include <fstream>
#include <sstream>

namespace minEngine
{
    bool Shader::ReadSourceFile(const std::filesystem::path& path, std::string& outSource, std::string* outError)
    {
        std::ifstream inputFile(path, std::ios::binary);
        if (!inputFile.is_open())
        {
            const std::string message = "Failed to open shader source file: " + path.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        std::ostringstream buffer;
        buffer << inputFile.rdbuf();
        outSource = buffer.str();
        return true;
    }

    std::filesystem::path Shader::EngineShaderPath(const char* fileName)
    {
        return std::filesystem::path("../Shaders") / fileName;
    }

    bool Shader::CompileFromSource(
        RHI& rhi,
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outError)
    {
        m_RHIShader.reset();
        m_CompileLog.clear();

        std::string compileLog;
        m_RHIShader = rhi.CreateRHIShader(vertexSource, fragmentSource, &compileLog);
        if (m_RHIShader == nullptr)
        {
            m_CompileLog = compileLog;
            if (outError != nullptr)
            {
                *outError = m_CompileLog;
            }
            return false;
        }

        return true;
    }

    bool Shader::CompileFromFiles(
        RHI& rhi,
        const std::filesystem::path& vertexShaderPath,
        const std::filesystem::path& fragmentShaderPath,
        std::string* outError)
    {
        std::string vertexSource;
        std::string fragmentSource;
        if (!ReadSourceFile(vertexShaderPath, vertexSource, outError))
        {
            return false;
        }

        if (!ReadSourceFile(fragmentShaderPath, fragmentSource, outError))
        {
            return false;
        }

        return CompileFromSource(rhi, vertexSource, fragmentSource, outError);
    }

    std::shared_ptr<Shader> Shader::CreateFromSource(
        RHI& rhi,
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outError)
    {
        std::shared_ptr<Shader> shader = std::make_shared<Shader>();
        if (!shader->CompileFromSource(rhi, vertexSource, fragmentSource, outError))
        {
            return nullptr;
        }

        return shader;
    }

    std::shared_ptr<Shader> Shader::CreateFromFiles(
        RHI& rhi,
        const std::filesystem::path& vertexShaderPath,
        const std::filesystem::path& fragmentShaderPath,
        std::string* outError)
    {
        std::shared_ptr<Shader> shader = std::make_shared<Shader>();
        if (!shader->CompileFromFiles(rhi, vertexShaderPath, fragmentShaderPath, outError))
        {
            return nullptr;
        }

        return shader;
    }

    bool Shader::TryCompileSourcesOnGpu(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outError)
    {
        const std::shared_ptr<OpenGLShader> shader = std::make_shared<OpenGLShader>(vertexSource, fragmentSource);
        if (shader->IsValid())
        {
            return true;
        }

        if (outError != nullptr)
        {
            *outError = shader->GetCompileLog();
        }
        return false;
    }
}

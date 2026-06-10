#include "EngineShaderUtils.h"

#include "OpenGL/OpenGLRHIResources.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"

#include <fstream>
#include <sstream>

namespace minEngine
{
    namespace EngineShaderUtils
    {
        bool ReadShaderSourceFile(
            const std::filesystem::path& path,
            std::string& outSource,
            std::string* outError)
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

        std::filesystem::path EngineShaderPath(const char* fileName)
        {
            return std::filesystem::path("../Shaders") / fileName;
        }

        RHIShaderRef CreateShaderFromFiles(
            RHI& rhi,
            const std::filesystem::path& vertexShaderPath,
            const std::filesystem::path& fragmentShaderPath,
            std::string* outError)
        {
            std::string vertexSource;
            std::string fragmentSource;
            if (!ReadShaderSourceFile(vertexShaderPath, vertexSource, outError))
            {
                return nullptr;
            }

            if (!ReadShaderSourceFile(fragmentShaderPath, fragmentSource, outError))
            {
                return nullptr;
            }

            return rhi.RHICreateShader(vertexSource, fragmentSource, outError);
        }

        bool TryCompileSourcesOnGpu(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outError)
        {
            const OpenGLRHIShader shader(vertexSource, fragmentSource);
            if (shader.IsValid())
            {
                return true;
            }

            if (outError != nullptr)
            {
                *outError = shader.GetCompileLog();
            }
            return false;
        }
    }
}

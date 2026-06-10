#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

#include <filesystem>
#include <memory>
#include <string>

namespace minEngine
{
    class RHI;
    class OpenGLShader;

    namespace EngineShaderUtils
    {
        bool ReadShaderSourceFile(
            const std::filesystem::path& path,
            std::string& outSource,
            std::string* outError = nullptr);

        RHIShaderRef CreateShaderFromFiles(
            RHI& rhi,
            const std::filesystem::path& vertexShaderPath,
            const std::filesystem::path& fragmentShaderPath,
            std::string* outError = nullptr);

        std::filesystem::path EngineShaderPath(const char* fileName);

        bool TryCompileSourcesOnGpu(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outError = nullptr);

        OpenGLShader* GetOpenGLShader(RHIShader* shader);
    }
}

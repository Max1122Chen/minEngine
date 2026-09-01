#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

#include <filesystem>
#include <memory>
#include <string>

namespace minEngine
{
    class RHI;

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

        /** GLSL files -> ShaderCompiler (backend SPIR-V target) -> RHICreateShader(bytecode). */
        RHIShaderRef CreateShaderFromSpirvFiles(
            RHI& rhi,
            const std::filesystem::path& vertexShaderPath,
            const std::filesystem::path& fragmentShaderPath,
            std::string* outError = nullptr);

        /** In-memory GLSL (e.g. MaterialCompiler output) -> SPIR-V -> RHICreateShader. */
        RHIShaderRef CreateShaderFromSpirvSources(
            RHI& rhi,
            const std::string& vertexSource,
            const std::string& fragmentSource,
            const std::string& debugName = {},
            std::string* outError = nullptr);

        std::filesystem::path EngineShaderPath(const char* fileName);

        bool TryCompileSourcesOnGpu(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outError = nullptr);
    }
}

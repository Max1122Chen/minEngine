#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace minEngine
{
    class RHI;

    namespace EngineShaderUtils
    {
        MINENGINE_API bool ReadShaderSourceFile(
            const std::filesystem::path& path,
            std::string& outSource,
            std::string* outError = nullptr);

        MINENGINE_API RHIShaderRef CreateShaderFromFiles(
            RHI& rhi,
            const std::filesystem::path& vertexShaderPath,
            const std::filesystem::path& fragmentShaderPath,
            std::string* outError = nullptr);

        /** GLSL files -> ShaderCompiler (backend SPIR-V target) -> RHICreateShader(bytecode). */
        MINENGINE_API RHIShaderRef CreateShaderFromSpirvFiles(
            RHI& rhi,
            const std::filesystem::path& vertexShaderPath,
            const std::filesystem::path& fragmentShaderPath,
            std::string* outError = nullptr);

        /**
         * In-memory GLSL (e.g. MaterialCompiler output) -> SPIR-V -> RHICreateShader.
         *
         * Memoized per RHI instance by (vertex source, fragment source, SPIR-V target): identical
         * generated GLSL reuses the same GPU program instead of re-linking one per Material.
         * Callers must not mutate the returned shader.
         */
        MINENGINE_API RHIShaderRef CreateShaderFromSpirvSources(
            RHI& rhi,
            const std::string& vertexSource,
            const std::string& fragmentSource,
            const std::string& debugName = {},
            std::string* outError = nullptr);

        /**
         * Drops memoized GPU programs (shader hot-reload / test isolation / RenderSystem::Shutdown).
         * Must run while the graphics context is alive: dropping the last reference destroys the
         * RHIShader, whose destructor calls glDeleteProgram.
         *
         * Lifetime compile statistics are NOT reset here (see ResetProgramCacheStats) so hit-rate
         * readings stay meaningful across invalidations.
         */
        MINENGINE_API void ClearProgramCache();
        /** Programs currently memoized for the most recent RHI instance. */
        MINENGINE_API size_t GetProgramCacheEntryCount();
        /** Cumulative program-cache hits since process start (diagnostic). */
        MINENGINE_API uint64_t GetProgramCacheHits();
        MINENGINE_API void ResetProgramCacheStats();

        MINENGINE_API std::filesystem::path EngineShaderPath(const char* fileName);

        MINENGINE_API bool TryCompileSourcesOnGpu(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outError = nullptr);
    }
}

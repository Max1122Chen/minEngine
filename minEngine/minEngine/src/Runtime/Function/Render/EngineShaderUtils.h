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

        /**
         * In-memory GLSL (e.g. MaterialCompiler output) -> SPIR-V -> RHICreateShader.
         *
         * Memoized per RHI instance by (vertex source, fragment source, SPIR-V target): identical
         * generated GLSL reuses the same GPU program instead of re-linking one per Material.
         * Callers must not mutate the returned shader.
         */
        RHIShaderRef CreateShaderFromSpirvSources(
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
        void ClearProgramCache();
        /** Programs currently memoized for the most recent RHI instance. */
        size_t GetProgramCacheEntryCount();
        /** Cumulative program-cache hits since process start (diagnostic). */
        uint64_t GetProgramCacheHits();
        void ResetProgramCacheStats();

        std::filesystem::path EngineShaderPath(const char* fileName);

        bool TryCompileSourcesOnGpu(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outError = nullptr);
    }
}

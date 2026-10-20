#pragma once

#include "Core.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    enum class ShaderSpirvTarget : uint8_t
    {
        Vulkan,
        OpenGL,
    };

    enum class ShaderCompilerStage : uint8_t
    {
        Vertex,
        Fragment,
    };

    struct ShaderCompileRequest
    {
        std::string Source;
        ShaderCompilerStage Stage = ShaderCompilerStage::Vertex;
        ShaderSpirvTarget Target = ShaderSpirvTarget::Vulkan;
        std::string EntryPoint = "main";
        /** Optional label for logs / cache keys (e.g. Present.vert). */
        std::string DebugName;
    };

    struct ShaderCompileResult
    {
        bool Success = false;
        std::vector<uint32_t> SpirvWords;
        std::string Log;
    };

    /** Compile-path counters; the measurable oracle for cache behaviour (MAT-F08). */
    struct ShaderCompileStats
    {
        uint64_t CompileRequests = 0;
        uint64_t MemoryCacheHits = 0;
        uint64_t DiskCacheHits = 0;
        /** Actual glslangValidator process launches — one per emitted "ShaderCompiler: compiled ..." line. */
        uint64_t GlslangInvocations = 0;
    };

    /**
     * GLSL -> SPIR-V via Vulkan SDK glslangValidator.
     * Produces separate Vulkan (-V) and OpenGL (-G) artifacts.
     *
     * Bytecode is memoized in-process by source key (stage/target/entry included), so identical
     * generated GLSL never re-launches glslang within a session. Artifacts are context-free and
     * therefore safe to share across RHI instances; GPU programs are not (see EngineShaderUtils).
     */
    class ShaderCompiler
    {
    public:
        static ShaderCompiler& Get();

        void SetGlslangValidatorPath(std::filesystem::path path);
        const std::filesystem::path& GetGlslangValidatorPath() const { return m_GlslangValidatorPath; }

        void SetCacheDirectory(std::filesystem::path path);
        const std::filesystem::path& GetCacheDirectory() const { return m_CacheDirectory; }

        bool IsAvailable(std::string* outError = nullptr) const;

        /** Resolve validator from explicit path, VULKAN_SDK, or PATH. */
        bool DiscoverGlslangValidator(std::string* outError = nullptr);

        ShaderCompileResult Compile(const ShaderCompileRequest& request);

        ShaderCompileStats GetCompileStats() const;
        void ResetCompileStats();
        /** Drops in-process bytecode (shader hot-reload / test isolation). Disk cache is untouched. */
        void ClearMemoryCache();
        size_t GetMemoryCacheEntryCount() const;

        ShaderCompileResult CompileFile(
            const std::filesystem::path& sourcePath,
            ShaderCompilerStage stage,
            ShaderSpirvTarget target);

        static const char* ToStageFlag(ShaderCompilerStage stage);
        static const char* ToTargetFlag(ShaderSpirvTarget target);
        static const char* ToTargetEnv(ShaderSpirvTarget target);

        /**
         * OpenGL SPIR-V requires DescriptorSet == 0; desktop GLSL has no `set=`.
         * Strip `set = N` from any `layout(...)` list (including `std140, set = …`),
         * keeping `binding = M` (flat GL binding / texture unit).
         * @param debugName Optional compile label (e.g. ShadowPass.vert) for pass-local set=0 remaps.
         */
        static std::string FlattenDescriptorSetsForOpenGL(
            const std::string& source,
            const std::string& debugName = {});

        /**
         * Inject clip-depth range define after `#version` for SPIR-V targets.
         * Vulkan: ZO depth read (MINENGINE_CLIP_DEPTH_ZERO_TO_ONE=1).
         */
        static std::string InjectClipSpaceDefines(const std::string& source, ShaderSpirvTarget target);

    private:
        ShaderCompiler() = default;

        bool EnsureCacheDirectory(std::string* outError) const;
        std::filesystem::path MakeCacheFilePath(
            const std::string& sourceKey,
            ShaderCompilerStage stage,
            ShaderSpirvTarget target) const;
        bool TryReadMemoryCache(const std::string& sourceKey, std::vector<uint32_t>& outWords);
        void StoreMemoryCache(const std::string& sourceKey, const std::vector<uint32_t>& words);
        static std::string HashSourceKey(const ShaderCompileRequest& request);
        static bool ReadSpirvFile(const std::filesystem::path& path, std::vector<uint32_t>& outWords, std::string* outError);
        static bool WriteTextFile(const std::filesystem::path& path, const std::string& text, std::string* outError);
        bool InvokeGlslang(
            const std::filesystem::path& inputGlslPath,
            const std::filesystem::path& outputSpvPath,
            ShaderCompilerStage stage,
            ShaderSpirvTarget target,
            const std::string& entryPoint,
            std::string* outLog) const;

        std::filesystem::path m_GlslangValidatorPath;
        std::filesystem::path m_CacheDirectory;

        mutable std::mutex m_MemoryCacheMutex;
        std::unordered_map<std::string, std::vector<uint32_t>> m_MemoryCache;
        mutable std::mutex m_StatsMutex;
        ShaderCompileStats m_Stats;
    };
}

#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Resource/Asset.h"

#include <filesystem>
#include <memory>
#include <string>

namespace minEngine
{
    class RHI;

    ME_CLASS()
    class Shader : public Asset
    {
        ME_GENERATED_BODY(Shader)
        friend class AssetManager;
    public:
        Shader() = default;
        ~Shader() = default;

        std::shared_ptr<RHIShaderLegacy> GetRHIShader() const { return m_RHIShader; }
        bool IsValid() const { return m_RHIShader != nullptr && m_RHIShader->IsValid(); }
        const std::string& GetCompileLog() const { return m_CompileLog; }

        bool CompileFromSource(
            RHI& rhi,
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outError = nullptr);

        bool CompileFromFiles(
            RHI& rhi,
            const std::filesystem::path& vertexShaderPath,
            const std::filesystem::path& fragmentShaderPath,
            std::string* outError = nullptr);

        static std::shared_ptr<Shader> CreateFromSource(
            RHI& rhi,
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outError = nullptr);

        static std::shared_ptr<Shader> CreateFromFiles(
            RHI& rhi,
            const std::filesystem::path& vertexShaderPath,
            const std::filesystem::path& fragmentShaderPath,
            std::string* outError = nullptr);

        // Shaders live next to bin/ (minEngine/Shaders) when cwd is minEngine/bin.
        static std::filesystem::path EngineShaderPath(const char* fileName);

        // Requires a current OpenGL context (material compile tests, tooling).
        static bool TryCompileSourcesOnGpu(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outError = nullptr);

    private:
        static bool ReadSourceFile(
            const std::filesystem::path& path,
            std::string& outSource,
            std::string* outError = nullptr);

        ME_PROPERTY()
        std::shared_ptr<RHIShaderLegacy> m_RHIShader;

        std::string m_CompileLog;
    };
}

#include "Shader.gen.h"

#include "EngineShaderUtils.h"

#include "OpenGL/OpenGLRHIResources.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/ShaderCompiler/ShaderCompiler.h"

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

        RHIShaderRef CreateShaderFromSpirvFiles(
            RHI& rhi,
            const std::filesystem::path& vertexShaderPath,
            const std::filesystem::path& fragmentShaderPath,
            std::string* outError)
        {
            ShaderCompiler& compiler = ShaderCompiler::Get();
            std::string discoverError;
            if (!compiler.DiscoverGlslangValidator(&discoverError))
            {
                if (outError != nullptr)
                {
                    *outError = discoverError;
                }
                ME_CORE_ERROR("{}", discoverError);
                return nullptr;
            }

            const ShaderSpirvTarget spirvTarget = RHIBackendSelection::IsVulkan()
                ? ShaderSpirvTarget::Vulkan
                : ShaderSpirvTarget::OpenGL;

            const ShaderCompileResult vertexResult =
                compiler.CompileFile(vertexShaderPath, ShaderCompilerStage::Vertex, spirvTarget);
            if (!vertexResult.Success)
            {
                if (outError != nullptr)
                {
                    *outError = vertexResult.Log;
                }
                ME_CORE_ERROR("SPIR-V vertex compile failed: {}", vertexResult.Log);
                return nullptr;
            }

            const ShaderCompileResult fragmentResult =
                compiler.CompileFile(fragmentShaderPath, ShaderCompilerStage::Fragment, spirvTarget);
            if (!fragmentResult.Success)
            {
                if (outError != nullptr)
                {
                    *outError = fragmentResult.Log;
                }
                ME_CORE_ERROR("SPIR-V fragment compile failed: {}", fragmentResult.Log);
                return nullptr;
            }

            RHIShaderCreateDesc desc;
            desc.DebugName = vertexShaderPath.filename().string() + "+" + fragmentShaderPath.filename().string();
            desc.Stages.push_back({RHIGraphicsShaderStage::Vertex, vertexResult.SpirvWords});
            desc.Stages.push_back({RHIGraphicsShaderStage::Pixel, fragmentResult.SpirvWords});
            return rhi.RHICreateShader(desc, outError);
        }

        RHIShaderRef CreateShaderFromSpirvSources(
            RHI& rhi,
            const std::string& vertexSource,
            const std::string& fragmentSource,
            const std::string& debugName,
            std::string* outError)
        {
            ShaderCompiler& compiler = ShaderCompiler::Get();
            std::string discoverError;
            if (!compiler.DiscoverGlslangValidator(&discoverError))
            {
                if (outError != nullptr)
                {
                    *outError = discoverError;
                }
                ME_CORE_ERROR("{}", discoverError);
                return nullptr;
            }

            const ShaderSpirvTarget spirvTarget = RHIBackendSelection::IsVulkan()
                ? ShaderSpirvTarget::Vulkan
                : ShaderSpirvTarget::OpenGL;

            const std::string label = debugName.empty() ? "Material" : debugName;

            ShaderCompileRequest vertexRequest;
            vertexRequest.Source = vertexSource;
            vertexRequest.Stage = ShaderCompilerStage::Vertex;
            vertexRequest.Target = spirvTarget;
            vertexRequest.DebugName = label + ".vert";
            const ShaderCompileResult vertexResult = compiler.Compile(vertexRequest);
            if (!vertexResult.Success)
            {
                if (outError != nullptr)
                {
                    *outError = vertexResult.Log;
                }
                ME_CORE_ERROR("SPIR-V vertex compile failed: {}", vertexResult.Log);
                return nullptr;
            }

            ShaderCompileRequest fragmentRequest;
            fragmentRequest.Source = fragmentSource;
            fragmentRequest.Stage = ShaderCompilerStage::Fragment;
            fragmentRequest.Target = spirvTarget;
            fragmentRequest.DebugName = label + ".frag";
            const ShaderCompileResult fragmentResult = compiler.Compile(fragmentRequest);
            if (!fragmentResult.Success)
            {
                if (outError != nullptr)
                {
                    *outError = fragmentResult.Log;
                }
                ME_CORE_ERROR("SPIR-V fragment compile failed: {}", fragmentResult.Log);
                return nullptr;
            }

            RHIShaderCreateDesc desc;
            desc.DebugName = label;
            desc.Stages.push_back({RHIGraphicsShaderStage::Vertex, vertexResult.SpirvWords});
            desc.Stages.push_back({RHIGraphicsShaderStage::Pixel, fragmentResult.SpirvWords});
            return rhi.RHICreateShader(desc, outError);
        }

        bool TryCompileSourcesOnGpu(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outError)
        {
            // Desktop GL string compile has no descriptor sets; match OpenGL SPIR-V flatten.
            const std::string flatVertex = ShaderCompiler::FlattenDescriptorSetsForOpenGL(vertexSource);
            const std::string flatFragment = ShaderCompiler::FlattenDescriptorSetsForOpenGL(fragmentSource);
            const OpenGLRHIShader shader(flatVertex, flatFragment);
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

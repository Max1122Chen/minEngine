#include "EngineShaderUtils.h"

#include "OpenGL/OpenGLRHIResources.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/ShaderCompiler/ShaderCompiler.h"

#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace minEngine
{
    namespace EngineShaderUtils
    {
        namespace
        {
            /** GPU programs are owned by one RHI instance; the cache is pruned when a new instance appears. */
            std::mutex g_ProgramCacheMutex;
            std::unordered_map<uint64_t, RHIShaderRef> g_ProgramCache;
            uint64_t g_ProgramCacheRHISerial = 0;
            uint64_t g_ProgramCacheHits = 0;

            uint64_t HashProgramSources(
                const std::string& vertexSource,
                const std::string& fragmentSource,
                ShaderSpirvTarget target)
            {
                // FNV-1a 64-bit over both stages + backend target (same scheme as ShaderCompiler keys).
                uint64_t hash = 14695981039346656037ull;
                auto mix = [&hash](const char* data, size_t size)
                {
                    for (size_t i = 0; i < size; ++i)
                    {
                        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
                        hash *= 1099511628211ull;
                    }
                };

                mix(vertexSource.data(), vertexSource.size());
                const char separator = '\0';
                mix(&separator, 1);
                mix(fragmentSource.data(), fragmentSource.size());
                const char targetTag = static_cast<char>(target);
                mix(&targetTag, 1);
                return hash;
            }
        }

        void ClearProgramCache()
        {
            std::lock_guard<std::mutex> lock(g_ProgramCacheMutex);
            g_ProgramCache.clear();
            g_ProgramCacheRHISerial = 0;
            // g_ProgramCacheHits deliberately survives: it is a lifetime diagnostic counter, and
            // resetting it on every invalidation would make hit-rate readings incomparable.
        }

        uint64_t GetProgramCacheHits()
        {
            std::lock_guard<std::mutex> lock(g_ProgramCacheMutex);
            return g_ProgramCacheHits;
        }

        void ResetProgramCacheStats()
        {
            std::lock_guard<std::mutex> lock(g_ProgramCacheMutex);
            g_ProgramCacheHits = 0;
        }

        size_t GetProgramCacheEntryCount()
        {
            std::lock_guard<std::mutex> lock(g_ProgramCacheMutex);
            return g_ProgramCache.size();
        }

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
                ME_LOG(LogRender, Error, "{}", message);
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
                ME_LOG(LogRender, Error, "{}", discoverError);
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
                ME_LOG(LogRender, Error, "SPIR-V vertex compile failed: {}", vertexResult.Log);
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
                ME_LOG(LogRender, Error, "SPIR-V fragment compile failed: {}", fragmentResult.Log);
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
                ME_LOG(LogRender, Error, "{}", discoverError);
                return nullptr;
            }

            const ShaderSpirvTarget spirvTarget = RHIBackendSelection::IsVulkan()
                ? ShaderSpirvTarget::Vulkan
                : ShaderSpirvTarget::OpenGL;

            const std::string label = debugName.empty() ? "Material" : debugName;
            const uint64_t programKey = HashProgramSources(vertexSource, fragmentSource, spirvTarget);

            {
                std::lock_guard<std::mutex> lock(g_ProgramCacheMutex);
                if (g_ProgramCacheRHISerial != rhi.GetInstanceSerial())
                {
                    // A new RHI instance owns its own GPU objects; drop the previous owner's programs.
                    g_ProgramCache.clear();
                    g_ProgramCacheRHISerial = rhi.GetInstanceSerial();
                }

                const auto found = g_ProgramCache.find(programKey);
                if (found != g_ProgramCache.end())
                {
                    ++g_ProgramCacheHits;
                    return found->second;
                }
            }

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
                ME_LOG(LogRender, Error, "SPIR-V vertex compile failed: {}", vertexResult.Log);
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
                ME_LOG(LogRender, Error, "SPIR-V fragment compile failed: {}", fragmentResult.Log);
                return nullptr;
            }

            RHIShaderCreateDesc desc;
            desc.DebugName = label;
            desc.Stages.push_back({RHIGraphicsShaderStage::Vertex, vertexResult.SpirvWords});
            desc.Stages.push_back({RHIGraphicsShaderStage::Pixel, fragmentResult.SpirvWords});
            RHIShaderRef shader = rhi.RHICreateShader(desc, outError);
            if (shader != nullptr)
            {
                std::lock_guard<std::mutex> lock(g_ProgramCacheMutex);
                g_ProgramCache[programKey] = shader;
            }
            return shader;
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

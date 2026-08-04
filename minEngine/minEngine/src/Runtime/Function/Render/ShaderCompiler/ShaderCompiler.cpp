#include "ShaderCompiler.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace minEngine
{
    ShaderCompiler& ShaderCompiler::Get()
    {
        static ShaderCompiler s_Instance;
        return s_Instance;
    }

    void ShaderCompiler::SetGlslangValidatorPath(std::filesystem::path path)
    {
        m_GlslangValidatorPath = std::move(path);
    }

    void ShaderCompiler::SetCacheDirectory(std::filesystem::path path)
    {
        m_CacheDirectory = std::move(path);
    }

    const char* ShaderCompiler::ToStageFlag(ShaderCompilerStage stage)
    {
        switch (stage)
        {
            case ShaderCompilerStage::Vertex:
                return "vert";
            case ShaderCompilerStage::Fragment:
                return "frag";
            default:
                return "vert";
        }
    }

    const char* ShaderCompiler::ToTargetFlag(ShaderSpirvTarget target)
    {
        return target == ShaderSpirvTarget::OpenGL ? "-G" : "-V";
    }

    const char* ShaderCompiler::ToTargetEnv(ShaderSpirvTarget target)
    {
        return target == ShaderSpirvTarget::OpenGL ? "opengl" : "vulkan1.2";
    }

    bool ShaderCompiler::DiscoverGlslangValidator(std::string* outError)
    {
        if (!m_GlslangValidatorPath.empty() && std::filesystem::exists(m_GlslangValidatorPath))
        {
            return true;
        }

#if defined(MINENGINE_GLSLANG_VALIDATOR)
        {
            const std::filesystem::path configured(MINENGINE_GLSLANG_VALIDATOR);
            if (std::filesystem::exists(configured))
            {
                m_GlslangValidatorPath = configured;
                return true;
            }
        }
#endif

        if (const char* vulkanSdk = std::getenv("VULKAN_SDK"))
        {
            const std::filesystem::path sdkRoot(vulkanSdk);
            const std::array<std::filesystem::path, 4> candidates = {
                sdkRoot / "Bin" / "glslangValidator.exe",
                sdkRoot / "Bin" / "glslang.exe",
                sdkRoot / "bin" / "glslangValidator",
                sdkRoot / "bin" / "glslang",
            };
            for (const std::filesystem::path& candidate : candidates)
            {
                if (std::filesystem::exists(candidate))
                {
                    m_GlslangValidatorPath = candidate;
                    return true;
                }
            }
        }

        if (outError != nullptr)
        {
            *outError =
                "glslangValidator not found. Set VULKAN_SDK or configure MINENGINE_GLSLANG_VALIDATOR "
                "(RND-F05 ShaderCompiler).";
        }
        return false;
    }

    bool ShaderCompiler::IsAvailable(std::string* outError) const
    {
        if (!m_GlslangValidatorPath.empty() && std::filesystem::exists(m_GlslangValidatorPath))
        {
            return true;
        }

        // Non-const discover path for convenience when callers only hold const ref is avoided —
        // callers should Discover first. Here we only report.
        if (outError != nullptr)
        {
            *outError = "ShaderCompiler: glslangValidator path is not set. Call DiscoverGlslangValidator().";
        }
        return false;
    }

    bool ShaderCompiler::EnsureCacheDirectory(std::string* outError) const
    {
        if (m_CacheDirectory.empty())
        {
            return true;
        }

        std::error_code errorCode;
        std::filesystem::create_directories(m_CacheDirectory, errorCode);
        if (errorCode)
        {
            if (outError != nullptr)
            {
                *outError = "Failed to create shader cache directory: " + m_CacheDirectory.string();
            }
            return false;
        }
        return true;
    }

    std::string ShaderCompiler::HashSourceKey(const ShaderCompileRequest& request)
    {
        // FNV-1a 64-bit over source + stage/target/entry — enough for local cache keys.
        uint64_t hash = 14695981039346656037ull;
        auto mix = [&hash](const char* data, size_t size)
        {
            for (size_t i = 0; i < size; ++i)
            {
                hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
                hash *= 1099511628211ull;
            }
        };

        mix(request.Source.data(), request.Source.size());
        const char stage = static_cast<char>(request.Stage);
        const char target = static_cast<char>(request.Target);
        mix(&stage, 1);
        mix(&target, 1);
        mix(request.EntryPoint.data(), request.EntryPoint.size());

        std::ostringstream stream;
        stream << std::hex << hash;
        return stream.str();
    }

    std::filesystem::path ShaderCompiler::MakeCacheFilePath(const ShaderCompileRequest& request) const
    {
        const char* targetTag = request.Target == ShaderSpirvTarget::OpenGL ? "gl" : "vk";
        const char* stageTag = ToStageFlag(request.Stage);
        return m_CacheDirectory / (HashSourceKey(request) + "." + stageTag + "." + targetTag + ".spv");
    }

    bool ShaderCompiler::ReadSpirvFile(
        const std::filesystem::path& path,
        std::vector<uint32_t>& outWords,
        std::string* outError)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
        {
            if (outError != nullptr)
            {
                *outError = "Failed to open SPIR-V file: " + path.string();
            }
            return false;
        }

        input.seekg(0, std::ios::end);
        const std::streamoff byteCount = input.tellg();
        input.seekg(0, std::ios::beg);
        if (byteCount <= 0 || (byteCount % 4) != 0)
        {
            if (outError != nullptr)
            {
                *outError = "Invalid SPIR-V size: " + path.string();
            }
            return false;
        }

        outWords.resize(static_cast<size_t>(byteCount) / 4u);
        input.read(reinterpret_cast<char*>(outWords.data()), byteCount);
        if (!input)
        {
            if (outError != nullptr)
            {
                *outError = "Failed to read SPIR-V file: " + path.string();
            }
            outWords.clear();
            return false;
        }
        return true;
    }

    bool ShaderCompiler::WriteTextFile(
        const std::filesystem::path& path,
        const std::string& text,
        std::string* outError)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            if (outError != nullptr)
            {
                *outError = "Failed to write temp GLSL: " + path.string();
            }
            return false;
        }
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(output);
    }

    bool ShaderCompiler::InvokeGlslang(
        const std::filesystem::path& inputGlslPath,
        const std::filesystem::path& outputSpvPath,
        ShaderCompilerStage stage,
        ShaderSpirvTarget target,
        const std::string& entryPoint,
        std::string* outLog) const
    {
#if defined(_WIN32)
        std::ostringstream command;
        command << '"' << m_GlslangValidatorPath.string() << '"'
                << ' ' << ToTargetFlag(target)
                << " --target-env " << ToTargetEnv(target)
                << " -S " << ToStageFlag(stage)
                << " -e " << entryPoint
                << " -o \"" << outputSpvPath.string() << '"'
                << " \"" << inputGlslPath.string() << '"';

        SECURITY_ATTRIBUTES securityAttributes{};
        securityAttributes.nLength = sizeof(securityAttributes);
        securityAttributes.bInheritHandle = TRUE;

        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;
        if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 0))
        {
            if (outLog != nullptr)
            {
                *outLog = "ShaderCompiler: CreatePipe failed.";
            }
            return false;
        }
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdOutput = writePipe;
        startupInfo.hStdError = writePipe;
        startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION processInfo{};
        std::string mutableCommand = command.str();
        const BOOL created = CreateProcessA(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo);

        CloseHandle(writePipe);
        writePipe = nullptr;

        if (!created)
        {
            CloseHandle(readPipe);
            if (outLog != nullptr)
            {
                *outLog = "ShaderCompiler: failed to launch glslangValidator: " + mutableCommand;
            }
            return false;
        }

        std::string captured;
        std::array<char, 512> buffer{};
        DWORD bytesRead = 0;
        while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0)
        {
            captured.append(buffer.data(), buffer.data() + bytesRead);
        }

        WaitForSingleObject(processInfo.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        CloseHandle(readPipe);

        if (outLog != nullptr)
        {
            *outLog = captured;
        }
        return exitCode == 0;
#else
        (void)inputGlslPath;
        (void)outputSpvPath;
        (void)stage;
        (void)target;
        (void)entryPoint;
        if (outLog != nullptr)
        {
            *outLog = "ShaderCompiler: non-Windows process launch not implemented yet.";
        }
        return false;
#endif
    }

    ShaderCompileResult ShaderCompiler::Compile(const ShaderCompileRequest& request)
    {
        ShaderCompileResult result;
        if (request.Source.empty())
        {
            result.Log = "ShaderCompiler: empty source.";
            return result;
        }

        std::string discoverError;
        if (!DiscoverGlslangValidator(&discoverError))
        {
            result.Log = discoverError;
            return result;
        }

        if (!EnsureCacheDirectory(&result.Log))
        {
            return result;
        }

        if (!m_CacheDirectory.empty())
        {
            const std::filesystem::path cachePath = MakeCacheFilePath(request);
            if (std::filesystem::exists(cachePath))
            {
                if (ReadSpirvFile(cachePath, result.SpirvWords, &result.Log))
                {
                    result.Success = true;
                    result.Log = "ShaderCompiler: cache hit " + cachePath.string();
                    return result;
                }
                result.SpirvWords.clear();
            }
        }

        const std::filesystem::path tempRoot = std::filesystem::temp_directory_path() / "minEngineShaderCompiler";
        std::error_code errorCode;
        std::filesystem::create_directories(tempRoot, errorCode);
        if (errorCode)
        {
            result.Log = "ShaderCompiler: failed to create temp directory.";
            return result;
        }

        const std::string key = HashSourceKey(request);
        const std::filesystem::path tempGlsl = tempRoot / (key + ".glsl");
        const std::filesystem::path tempSpv = tempRoot / (key + ".spv");

        if (!WriteTextFile(tempGlsl, request.Source, &result.Log))
        {
            return result;
        }

        std::string toolLog;
        const bool ok = InvokeGlslang(
            tempGlsl,
            tempSpv,
            request.Stage,
            request.Target,
            request.EntryPoint.empty() ? "main" : request.EntryPoint,
            &toolLog);
        result.Log = toolLog;

        if (!ok)
        {
            ME_CORE_ERROR(
                "ShaderCompiler: glslang failed ({} / {}): {}",
                request.DebugName.empty() ? "<memory>" : request.DebugName,
                ToTargetFlag(request.Target),
                toolLog);
            std::filesystem::remove(tempGlsl, errorCode);
            std::filesystem::remove(tempSpv, errorCode);
            return result;
        }

        if (!ReadSpirvFile(tempSpv, result.SpirvWords, &result.Log))
        {
            std::filesystem::remove(tempGlsl, errorCode);
            std::filesystem::remove(tempSpv, errorCode);
            return result;
        }

        if (!m_CacheDirectory.empty())
        {
            const std::filesystem::path cachePath = MakeCacheFilePath(request);
            std::filesystem::copy_file(
                tempSpv,
                cachePath,
                std::filesystem::copy_options::overwrite_existing,
                errorCode);
            if (errorCode)
            {
                ME_CORE_WARN("ShaderCompiler: failed to write cache '{}'", cachePath.string());
            }
        }

        std::filesystem::remove(tempGlsl, errorCode);
        std::filesystem::remove(tempSpv, errorCode);

        result.Success = true;
        if (!request.DebugName.empty())
        {
            ME_CORE_INFO(
                "ShaderCompiler: compiled {} -> {} ({} words)",
                request.DebugName,
                ToTargetFlag(request.Target),
                result.SpirvWords.size());
        }
        return result;
    }

    ShaderCompileResult ShaderCompiler::CompileFile(
        const std::filesystem::path& sourcePath,
        ShaderCompilerStage stage,
        ShaderSpirvTarget target)
    {
        ShaderCompileResult result;
        std::ifstream input(sourcePath, std::ios::binary);
        if (!input.is_open())
        {
            result.Log = "Failed to open shader source: " + sourcePath.string();
            return result;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();

        ShaderCompileRequest request;
        request.Source = buffer.str();
        request.Stage = stage;
        request.Target = target;
        request.DebugName = sourcePath.filename().string();
        return Compile(request);
    }
}

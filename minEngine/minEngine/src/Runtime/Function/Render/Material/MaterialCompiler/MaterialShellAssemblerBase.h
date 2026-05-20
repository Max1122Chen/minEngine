#pragma once

#include "MaterialCompileTypes.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace minEngine
{
    // Shared template/include I/O and anchor substitution for shell assembler implementations.
    class MaterialShellAssemblerBase
    {
    protected:
        static std::filesystem::path ResolveEngineDefaultAssetsRoot(const MaterialCompileEnvironment& env);

        static const char* ShaderLanguageSubdir(const MaterialCompileEnvironment& env);

        static bool LoadTemplateFile(
            const std::filesystem::path& engineDefaultAssetsRoot,
            const MaterialCompileEnvironment& env,
            const char* fileName,
            std::string& outText,
            MaterialCompileResult& compiled);

        static bool LoadIncludeFile(
            const std::filesystem::path& engineDefaultAssetsRoot,
            const MaterialCompileEnvironment& env,
            const char* relativePath,
            std::string& outText,
            MaterialCompileResult& compiled);

        static bool ApplyAnchors(
            std::string& inOutTemplate,
            const std::vector<std::pair<std::string, std::string>>& anchorsInOrder,
            MaterialCompileResult& compiled);

        static bool AssembleStageFromTemplate(
            const std::filesystem::path& engineDefaultAssetsRoot,
            const MaterialCompileEnvironment& env,
            const char* templateFileName,
            const std::vector<std::pair<std::string, std::string>>& anchors,
            std::string& outFullShader,
            MaterialCompileResult& compiled);

        static std::string EnsureTrailingNewline(std::string text);
    };
}

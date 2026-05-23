#include "MaterialShellAssemblerBase.h"

#include "Runtime/Core/Paths/PathRegistry.h"

#include <fstream>
#include <sstream>

namespace minEngine
{
    namespace
    {
        std::string MakePlaceholder(const std::string& key)
        {
            return std::string("@ME_INSERT:") + key + "@";
        }

        bool ReadTextFile(const std::filesystem::path& path, std::string& outText, MaterialCompileResult& compiled)
        {
            std::ifstream inputFile(path, std::ios::binary);
            if (!inputFile.is_open())
            {
                compiled.Diagnostics.push_back({
                    MaterialCompileDiagnostic::Error,
                    "Failed to open shader file: " + path.string(),
                });
                return false;
            }

            std::ostringstream buffer;
            buffer << inputFile.rdbuf();
            outText = buffer.str();
            return true;
        }
    }

    std::filesystem::path MaterialShellAssemblerBase::ResolveEngineDefaultAssetsRoot(const MaterialCompileEnvironment& env)
    {
        if (!env.EngineDefaultAssetsRootOverride.empty())
        {
            return std::filesystem::path(env.EngineDefaultAssetsRootOverride);
        }

        return PathRegistry::Get().GetEngineDefaultAssetsRoot();
    }

    const char* MaterialShellAssemblerBase::ShaderLanguageSubdir(const MaterialCompileEnvironment& env)
    {
        switch (env.ShaderLanguage)
        {
        case MaterialShaderLanguage::GLSL:
            return "GLSL";
        default:
            return nullptr;
        }
    }

    bool MaterialShellAssemblerBase::LoadTemplateFile(
        const std::filesystem::path& engineDefaultAssetsRoot,
        const MaterialCompileEnvironment& env,
        const char* fileName,
        std::string& outText,
        MaterialCompileResult& compiled)
    {
        if (engineDefaultAssetsRoot.empty())
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "EngineDefaultAssetsRoot is empty; cannot load material shader template (check EngineConfig).",
            });
            return false;
        }

        const char* languageDir = ShaderLanguageSubdir(env);
        if (languageDir == nullptr || languageDir[0] == '\0')
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "MaterialShaderLanguage is not supported for shader template loading.",
            });
            return false;
        }

        const std::filesystem::path path =
            std::filesystem::path(engineDefaultAssetsRoot) / "Shaders" / "Template" / languageDir / fileName;
        if (!std::filesystem::exists(path))
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Material shader template not found: " + path.string(),
            });
            return false;
        }

        return ReadTextFile(path, outText, compiled);
    }

    bool MaterialShellAssemblerBase::LoadIncludeFile(
        const std::filesystem::path& engineDefaultAssetsRoot,
        const MaterialCompileEnvironment& env,
        const char* relativePath,
        std::string& outText,
        MaterialCompileResult& compiled)
    {
        if (engineDefaultAssetsRoot.empty())
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "EngineDefaultAssetsRoot is empty; cannot load shader include (check EngineConfig).",
            });
            return false;
        }

        const char* languageDir = ShaderLanguageSubdir(env);
        if (languageDir == nullptr || languageDir[0] == '\0')
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "MaterialShaderLanguage is not supported for shader include loading.",
            });
            return false;
        }

        const std::filesystem::path path =
            std::filesystem::path(engineDefaultAssetsRoot) / "Shaders" / "Include" / languageDir / relativePath;
        if (!std::filesystem::exists(path))
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Shader include not found: " + path.string(),
            });
            return false;
        }

        if (!ReadTextFile(path, outText, compiled))
        {
            return false;
        }

        if (!outText.empty() && outText.back() != '\n')
        {
            outText += '\n';
        }
        if (!outText.empty())
        {
            outText += '\n';
        }
        return true;
    }

    bool MaterialShellAssemblerBase::ApplyAnchors(
        std::string& inOutTemplate,
        const std::vector<std::pair<std::string, std::string>>& anchorsInOrder,
        MaterialCompileResult& compiled)
    {
        for (const auto& entry : anchorsInOrder)
        {
            const std::string placeholder = MakePlaceholder(entry.first);
            if (inOutTemplate.find(placeholder) == std::string::npos)
            {
                compiled.Diagnostics.push_back({
                    MaterialCompileDiagnostic::Error,
                    "Shader template missing anchor: " + placeholder,
                });
                return false;
            }
        }

        for (const auto& entry : anchorsInOrder)
        {
            const std::string& key = entry.first;
            const std::string& value = entry.second;
            const std::string placeholder = MakePlaceholder(key);

            size_t searchPosition = 0;
            while ((searchPosition = inOutTemplate.find(placeholder, searchPosition)) != std::string::npos)
            {
                inOutTemplate.replace(searchPosition, placeholder.length(), value);
                searchPosition += value.length();
            }
        }

        const std::string marker = "@ME_INSERT:";
        if (inOutTemplate.find(marker) != std::string::npos)
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Shader template still contains unsubstituted @ME_INSERT:*@ anchors after assembly.",
            });
            return false;
        }

        return true;
    }

    bool MaterialShellAssemblerBase::AssembleStageFromTemplate(
        const std::filesystem::path& engineDefaultAssetsRoot,
        const MaterialCompileEnvironment& env,
        const char* templateFileName,
        const std::vector<std::pair<std::string, std::string>>& anchors,
        std::string& outFullShader,
        MaterialCompileResult& compiled)
    {
        std::string templateText;
        if (!LoadTemplateFile(engineDefaultAssetsRoot, env, templateFileName, templateText, compiled))
        {
            return false;
        }

        if (!ApplyAnchors(templateText, anchors, compiled))
        {
            return false;
        }

        outFullShader = std::move(templateText);
        return true;
    }

    std::string MaterialShellAssemblerBase::EnsureTrailingNewline(std::string text)
    {
        if (!text.empty() && text.back() != '\n')
        {
            text += '\n';
        }
        return text;
    }
}

#include "ProjectManager.h"

#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/JsonArchive.h"

#include <fstream>
#include <system_error>

namespace minEngine
{
    ProjectManager& ProjectManager::Get()
    {
        return *RuntimeGlobalContext::GetRuntimeGlobalContext().m_ProjectManager;
    }

    void ProjectManager::Initialize()
    {
        m_CurrentProject.Reset();
    }

    void ProjectManager::Shutdown()
    {
        CloseProject();
    }

    ProjectOpenResult ProjectManager::OpenProject(const std::filesystem::path& projectRoot)
    {
        CloseProject();

        if (projectRoot.empty())
        {
            return ProjectOpenResult(ProjectOpenStatus::InvalidProjectRoot, "Project root is empty.");
        }

        std::error_code absolutePathError;
        std::filesystem::path normalizedInputPath = std::filesystem::absolute(projectRoot, absolutePathError);
        if (absolutePathError)
        {
            return ProjectOpenResult(ProjectOpenStatus::InvalidProjectRoot,
                                     "Failed to resolve input path to absolute path.");
        }
        normalizedInputPath = normalizedInputPath.lexically_normal();

        std::filesystem::path normalizedProjectRoot;
        std::filesystem::path descriptorPath;

        std::error_code directoryCheckError;
        const bool inputIsDirectory = std::filesystem::is_directory(normalizedInputPath, directoryCheckError);
        if (!directoryCheckError && inputIsDirectory)
        {
            normalizedProjectRoot = normalizedInputPath;
            descriptorPath = BuildProjectDescPath(normalizedProjectRoot);
        }
        else
        {
            std::error_code fileCheckError;
            const bool inputIsFile = std::filesystem::is_regular_file(normalizedInputPath, fileCheckError);
            if (!fileCheckError
                && inputIsFile
                && normalizedInputPath.extension() == kProjectDescriptorExtension)
            {
                descriptorPath = normalizedInputPath;
                normalizedProjectRoot = descriptorPath.parent_path();
            }
            else
            {
                return ProjectOpenResult(
                    ProjectOpenStatus::InvalidProjectRoot,
                    "Input path must be a project directory or a .meproject descriptor file.");
            }
        }

        if (!IsLikelyProjectRoot(normalizedProjectRoot))
        {
            return ProjectOpenResult(ProjectOpenStatus::InvalidProjectRoot,
                                     "Project root is not a valid directory.");
        }

        if (descriptorPath.empty())
        {
            return ProjectOpenResult(ProjectOpenStatus::DescriptorNotFound,
                                     "Project descriptor path is empty.");
        }

        std::error_code existsError;
        const bool descriptorExists = std::filesystem::exists(descriptorPath, existsError);
        if (existsError || !descriptorExists)
        {
            return ProjectOpenResult(ProjectOpenStatus::DescriptorNotFound,
                                     "Project descriptor file does not exist.");
        }

        ProjectDescriptor descriptor;
        std::string loadError;
        if (!LoadProjectDesc(descriptorPath, descriptor, &loadError))
        {
            return ProjectOpenResult(ProjectOpenStatus::InvalidDescriptor,
                                     std::string("Failed to load project descriptor: ") + loadError);
        }

        if (descriptor.SchemaVersion == 0)
        {
            return ProjectOpenResult(ProjectOpenStatus::InvalidDescriptor,
                                     "Project descriptor schemaVersion must be greater than zero.");
        }

        if (descriptor.ProjectName.empty())
        {
            return ProjectOpenResult(ProjectOpenStatus::InvalidDescriptor,
                              "Project descriptor projectName is empty.");
        }

        if (!descriptor.ProjectId.IsValid())
        {
            return ProjectOpenResult(ProjectOpenStatus::InvalidDescriptor,
                              "Project descriptor projectId is invalid.");
        }

        ProjectContext openedProject;
        openedProject.IsOpened = true;
        openedProject.ProjectRoot = normalizedProjectRoot;
        openedProject.ProjectFile = descriptorPath;

        if (descriptor.ContentRoot.empty())
        {
            descriptor.ContentRoot = std::filesystem::path("Assets").string();
            openedProject.Diagnostics.emplace_back(
                "Descriptor contentRoot is empty; defaulting to 'Assets'.");
        }

        if (descriptor.ConfigRoot.empty())
        {
            descriptor.ConfigRoot = std::filesystem::path("Config").string();
            openedProject.Diagnostics.emplace_back(
                "Descriptor configRoot is empty; defaulting to 'Config'.");
        }

        openedProject.Descriptor = descriptor;

        const std::filesystem::path resolvedContentRoot = ResolveProjectPath(
            normalizedProjectRoot,
            openedProject.Descriptor.ContentRoot);
        if (resolvedContentRoot.empty())
        {
            openedProject.Diagnostics.emplace_back(
                "Resolved content root path is empty.");
        }
        else
        {
            std::error_code contentRootCheckError;
            const bool contentRootExists = std::filesystem::exists(resolvedContentRoot, contentRootCheckError);
            const bool contentRootIsDirectory =
                contentRootExists && std::filesystem::is_directory(resolvedContentRoot, contentRootCheckError);

            if (!contentRootCheckError && contentRootIsDirectory)
            {
                AssetManager::Get().ScanAssets(resolvedContentRoot.string());
                openedProject.Diagnostics.emplace_back(
                    std::string("Scanned project content root: ") + resolvedContentRoot.generic_string());
            }
            else
            {
                openedProject.Diagnostics.emplace_back(
                    std::string("Project content root not found or not a directory: ")
                    + resolvedContentRoot.generic_string());
            }
        }

        std::filesystem::path resolvedProjectScene = ResolveProjectPath(
            normalizedProjectRoot,
            openedProject.Descriptor.EditorDefaultScene);
        if (!resolvedProjectScene.empty())
        {
            std::error_code sceneExistsError;
            const bool sceneExists = std::filesystem::exists(resolvedProjectScene, sceneExistsError);
            if (!sceneExistsError && sceneExists)
            {
                openedProject.ResolvedEditorStartupScene = resolvedProjectScene;
            }
            else
            {
                openedProject.Diagnostics.emplace_back(
                    std::string("Project default scene missing, fallback to engine default: ")
                    + resolvedProjectScene.generic_string());
            }
        }
        else
        {
            openedProject.Diagnostics.emplace_back(
                "Descriptor editorDefaultScene is empty; fallback to engine default scene.");
        }

        if (openedProject.ResolvedEditorStartupScene.empty())
        {
            openedProject.ResolvedEditorStartupScene = m_EngineDefaultScenePath;
        }

        m_CurrentProject = std::move(openedProject);

        return ProjectOpenResult(ProjectOpenStatus::Success,
                     std::string("Project opened successfully: ") + m_CurrentProject.Descriptor.ProjectName);
    }

    void ProjectManager::CloseProject()
    {
        m_CurrentProject.Reset();
        // TODO: add more cleanup if needed (e.g. unload assets, scenes, etc.)
    }

    std::filesystem::path ProjectManager::ResolveEditorStartupScenePath() const
    {
        if (!m_CurrentProject.ResolvedEditorStartupScene.empty())
        {
            return m_CurrentProject.ResolvedEditorStartupScene;
        }

        return m_EngineDefaultScenePath;
    }

    bool ProjectManager::IsLikelyProjectRoot(const std::filesystem::path& projectRoot) const
    {
        if (projectRoot.empty())
        {
            return false;
        }

        std::error_code directoryError;
        const bool isDirectory = std::filesystem::is_directory(projectRoot, directoryError);
        return !directoryError && isDirectory;
    }

    std::filesystem::path ProjectManager::BuildProjectDescPath(const std::filesystem::path& projectRoot) const
    {
        if (projectRoot.empty())
        {
            return std::filesystem::path();
        }

        return projectRoot / kDefaultProjectDescriptorFileName;
    }

    std::filesystem::path ProjectManager::ResolveProjectPath(const std::filesystem::path &projectRoot, const std::string &configuredPath) const
    {
        if (configuredPath.empty())
        {
            return std::filesystem::path();
        }

        const std::filesystem::path pathValue(configuredPath);
        if (pathValue.is_absolute())
        {
            return pathValue.lexically_normal();
        }

        return (projectRoot / pathValue).lexically_normal();
    }

    bool ProjectManager::LoadProjectDesc(const std::filesystem::path& descriptorPath,
                                         ProjectDescriptor& outDesc,
                                         std::string* outErrorMessage) const
    {
        if (outErrorMessage != nullptr)
        {
            outErrorMessage->clear();
        }

        std::ifstream inputFile(descriptorPath);
        if (!inputFile.is_open())
        {
            if (outErrorMessage != nullptr)
            {
                *outErrorMessage = "Failed to open descriptor file.";
            }
            ME_CORE_ERROR("Failed to open project descriptor file '{}'", descriptorPath.string());
            return false;
        }

        Json jsonDesc;
        try
        {
            inputFile >> jsonDesc;
        }
        catch (const std::exception& e)
        {
            if (outErrorMessage != nullptr)
            {
                *outErrorMessage = std::string("Failed to parse descriptor JSON: ") + e.what();
            }
            ME_CORE_ERROR("Failed to parse project descriptor file '{}': {}", descriptorPath.string(), e.what());
            return false;
        }

        if (!jsonDesc.is_object())
        {
            if (outErrorMessage != nullptr)
            {
                *outErrorMessage = "Descriptor JSON root must be an object.";
            }
            ME_CORE_ERROR("Invalid project descriptor json root for '{}': root is not an object.", descriptorPath.string());
            return false;
        }

        const Json* descriptorNode = &jsonDesc;
        if (jsonDesc.contains("projectData") && jsonDesc["projectData"].is_object())
        {
            descriptorNode = &jsonDesc["projectData"];
        }

        Serialization::SerializerOptions options = {
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = false,
            .allowObjectPtrSerialization = true
        };

        Serialization::JsonReaderArchive archive(*descriptorNode);
        Serialization::SerializeResult deserializeResult = Serialization::Serializer::Deserialize(
            kProjectDescClassName,
            &outDesc,
            archive,
            options);

        if (!deserializeResult.ok)
        {
            if (outErrorMessage != nullptr)
            {
                *outErrorMessage = deserializeResult.message;
            }
            ME_CORE_ERROR("Failed to deserialize ProjectDescriptor from '{}': {}", descriptorPath.string(), deserializeResult.message);
            return false;
        }

        return true;
    }
}

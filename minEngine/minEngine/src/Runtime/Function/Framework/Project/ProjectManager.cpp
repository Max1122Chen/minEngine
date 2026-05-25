#include "ProjectManager.h"

#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/JsonArchive.h"

#include <fstream>
#include <system_error>

namespace minEngine
{
    ProjectManager* ProjectManager::s_Instance = nullptr;

    void ProjectManager::SetInstance(ProjectManager* instance)
    {
        s_Instance = instance;
    }

    ProjectManager& ProjectManager::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "ProjectManager is not initialized");
        return *s_Instance;
    }

    void ProjectManager::Initialize()
    {
    }

    void ProjectManager::Shutdown()
    {
        CloseCurrentProject();
        ME_CORE_INFO("ProjectManager Shutdown.");
    }

    ProjectOpenResult ProjectManager::OpenProject(const std::filesystem::path& projectRoot)
    {
        CloseCurrentProject();
        
        if (std::filesystem::exists(projectRoot))
        {
            std::filesystem::path descriptorPath;
            std::filesystem::path settingsPath;
            
            bool descriptorFound = false;
            bool settingsFound = false;
            // Handle the case where the path is a directory (e.g., C:/Projects/MyProject)
            if (std::filesystem::is_directory(projectRoot))
            {
                // Look for the project descriptor file (e.g., MyProject.meproject) in the directory
                for(auto& entry : std::filesystem::directory_iterator(projectRoot))
                {
                    if (entry.is_regular_file())
                    {
                        if (entry.path().extension() == kMEProjectExtension)
                        {
                            descriptorPath = entry.path();
                            descriptorFound = true;
                        }
                        if (entry.path().extension() == kMEProjectSettingsExtension)
                        {
                            settingsPath = entry.path();
                            settingsFound = true;
                        }
                    }
                }
                if (!descriptorFound)
                {
                    return ProjectOpenResult(ProjectOpenStatus::DescriptorNotFound, "Project descriptor file not found in the specified directory.");
                }
            }
            else
            {
                // Handle the case where the path is directly to a potential project descriptor file (e.g., C:/Projects/MyProject/MyProject.meproject)
                if (projectRoot.extension() == kMEProjectExtension)
                {
                    descriptorPath = projectRoot;

                    const std::filesystem::path descriptorDir = descriptorPath.parent_path();
                    if (!descriptorDir.empty() && std::filesystem::exists(descriptorDir) && std::filesystem::is_directory(descriptorDir))
                    {
                        for (auto& entry : std::filesystem::directory_iterator(descriptorDir))
                        {
                            if (entry.is_regular_file() && entry.path().extension() == kMEProjectSettingsExtension)
                            {
                                settingsPath = entry.path();
                                settingsFound = true;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    return ProjectOpenResult(ProjectOpenStatus::WrongDescriptorExtension, "Specified project file does not have the correct extension.");
                }
            }
            
            // Try to load and parse the project descriptor file
            ProjectDescriptor descriptor;
            bool descriptorLoaded = LoadProjectDesc(descriptorPath, descriptor);

            if (!descriptorLoaded)
            {
                return ProjectOpenResult(ProjectOpenStatus::InvalidDescriptor, "Failed to parse project descriptor file.");
            }

            // TODO: maybe we should also validate the content of the descriptor (e.g., check if required fields are present, if the project root path in the descriptor matches the actual project root, etc.)
            // TODO: later we may correct the project root path in the descriptor if it's missing or doesn't match the actual project root, to make it more robust against user errors
            

            // If everything is good, we can set the current project context (e.g., store the descriptor, load project-specific settings, etc.)
            m_CurrentProjectCtx.Descriptor = descriptor;

            m_CurrentSettingsPath = settingsFound ? settingsPath : ResolveDefaultSettingsPath(descriptorPath);

            // Try to load project-specific settings (e.g., MyProject.mesettings), but we won't fail opening the project if the settings file is missing or failed to parse, since the settings are optional and we can just use default settings in that case
            if (settingsFound)
            {
                ProjectSettings settings;
                bool settingsLoaded = LoadProjectSettings(settingsPath, settings);
                if (settingsLoaded)
                {
                    m_CurrentProjectCtx.Settings = settings;
                }
            }
            
            PathRegistry::Get().SetProjectRoots(std::filesystem::path(descriptor.ProjectRoot));

            const std::filesystem::path& assetsPath = PathRegistry::Get().GetProjectContentRoot();
            if (std::filesystem::exists(assetsPath) && std::filesystem::is_directory(assetsPath))
            {
                AssetManager::Get().ScanAssets(assetsPath);
                // TODO: maybe we should also load other types of project-specific data here (e.g., editorDefaultScene, editor settings, etc.)
            }
            else
            {
                ME_CORE_WARN("Assets directory not found in the project: {}", assetsPath.string());
            }
            return ProjectOpenResult(ProjectOpenStatus::Success, "Project " + descriptor.ProjectName + " opened successfully.");
        }
        else
        {
            return ProjectOpenResult(ProjectOpenStatus::PathNotFound, "Specified project path does not exist.");
        }
    }

    void ProjectManager::CloseCurrentProject()
    {
        if (AssetManager::HasInstance())
        {
            AssetManager::Get().ClearProjectRegistry();
        }

        m_CurrentProjectCtx.Reset();
        m_CurrentSettingsPath.clear();
        PathRegistry::Get().ClearProjectRoots();
    }

    std::filesystem::path ProjectManager::ResolveDefaultSettingsPath(
        const std::filesystem::path& descriptorPath) const
    {
        const std::filesystem::path descriptorDir = descriptorPath.parent_path();
        if (descriptorDir.empty())
        {
            return {};
        }

        const std::string stem = descriptorPath.stem().string();
        if (stem.empty())
        {
            return descriptorDir / ("Project" + std::string(kMEProjectSettingsExtension));
        }

        return descriptorDir / (stem + "Settings" + std::string(kMEProjectSettingsExtension));
    }

    bool ProjectManager::SaveCurrentProjectSettings()
    {
        if (m_CurrentSettingsPath.empty())
        {
            ME_CORE_WARN("SaveCurrentProjectSettings: no settings path (project not open).");
            return false;
        }

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            m_CurrentSettingsPath.string(),
            minEngine::Reflection::GetClassName<ProjectSettings>(),
            &m_CurrentProjectCtx.Settings,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = true,
                .allowObjectPtrSerialization = false});

        if (!result.ok)
        {
            ME_CORE_ERROR(
                "Failed to save project settings. Error: {}. Field path: {}. Settings file: {}",
                result.message,
                result.fieldPath,
                m_CurrentSettingsPath.string());
        }

        return result.ok;
    }

    bool ProjectManager::LoadProjectDesc(const std::filesystem::path &descriptorPath, ProjectDescriptor &outDescriptor)
    {
        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            descriptorPath.string(),
            minEngine::Reflection::GetClassName<ProjectDescriptor>(),
            &outDescriptor,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = true,
                .allowObjectPtrSerialization = false
            });

        if(!result.ok)
        {
            ME_CORE_ERROR("Failed to load project descriptor. Error: {}. Field path: {}. Descriptor file: {}",
                          result.message,
                          result.fieldPath,
                          descriptorPath.string());
        }
        return result.ok;
    }

    bool ProjectManager::LoadProjectSettings(const std::filesystem::path &settingsPath, ProjectSettings &outSettings)
    {
        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            settingsPath.string(),
            minEngine::Reflection::GetClassName<ProjectSettings>(),
            &outSettings,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = true,
                .allowObjectPtrSerialization = false
            });
        if(!result.ok)
        {
            ME_CORE_ERROR("Failed to load project settings. Error: {}. Field path: {}. Settings file: {}",
                          result.message,
                          result.fieldPath,
                          settingsPath.string());
        }
        return result.ok;
    }
}

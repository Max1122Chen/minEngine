#pragma once

#include "Core.h"
#include "ProjectDescriptor.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace minEngine
{
    enum class ProjectOpenStatus : uint8_t
    {
        Success = 0,
        InvalidProjectRoot,
        DescriptorNotFound,
        InvalidDescriptor,
        NotImplemented,
    };

    struct ProjectOpenResult
    {
        ProjectOpenResult() = default;
        ProjectOpenResult(ProjectOpenStatus inStatus, std::string inMessage)
            : Status(inStatus), Message(std::move(inMessage))
        {
        }
        ProjectOpenStatus Status = ProjectOpenStatus::NotImplemented;
        std::string Message;

        bool IsSuccess() const
        {
            return Status == ProjectOpenStatus::Success;
        }
    };

    struct ProjectContext
    {
        bool IsOpened = false;
        std::filesystem::path ProjectRoot;
        std::filesystem::path ProjectFile;
        ProjectDescriptor Descriptor;
        std::filesystem::path ResolvedEditorStartupScene;
        std::vector<std::string> Diagnostics;

        void Reset()
        {
            IsOpened = false;
            ProjectRoot.clear();
            ProjectFile.clear();
            Descriptor = ProjectDescriptor{};
            ResolvedEditorStartupScene.clear();
            Diagnostics.clear();
        }
    };

    class ProjectManager
    {
    public:
        ProjectManager() = default;
        ~ProjectManager() = default;

        static ProjectManager& Get();

        void Initialize();
        void Shutdown();

        ProjectOpenResult OpenProject(const std::filesystem::path& projectRoot);
        void CloseProject();

        bool HasOpenProject() const { return m_CurrentProject.IsOpened; }
        const ProjectContext& GetCurrentProject() const { return m_CurrentProject; }

        std::filesystem::path ResolveEditorStartupScenePath() const;
        const std::filesystem::path& GetEngineDefaultScenePath() const { return m_EngineDefaultScenePath; }

        bool IsLikelyProjectRoot(const std::filesystem::path& projectRoot) const;
        std::filesystem::path BuildProjectDescPath(const std::filesystem::path& projectRoot) const;

    public:
        static constexpr const char* kProjectDescriptorExtension = ".meproject";
        static constexpr const char* kAssetMetadataExtension = ".measset";
        static constexpr const char* kDefaultProjectDescriptorFileName = "Project.meproject";

    private:
        std::filesystem::path ResolveProjectPath(const std::filesystem::path& projectRoot, const std::string& configuredPath) const;
        bool LoadProjectDesc(const std::filesystem::path& descriptorPath, ProjectDescriptor& outDescriptor, std::string* outErrorMessage) const;

    private:
        static constexpr const char* kProjectDescClassName = "minEngine::ProjectDescriptor";
        ProjectContext m_CurrentProject;
        std::filesystem::path m_EngineDefaultScenePath{"Assets/Scenes/EditorDefault.scene.json"};
    };
}

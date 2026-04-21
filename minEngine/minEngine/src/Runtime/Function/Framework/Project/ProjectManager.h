#pragma once

#include "Core.h"
#include "ProjectDescriptor.h"
#include "ProjectSettings.h"

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
        PathNotFound,           // The specified path does not exist
        DescriptorNotFound,     // No project descriptor file found at the specified path
        WrongDescriptorExtension,  // Project descriptor file has wrong extension
        InvalidDescriptor,      // Project descriptor file is found but failed to parse or has invalid content
    };

    struct ProjectOpenResult
    {
        ProjectOpenResult() = default;
        ProjectOpenResult(ProjectOpenStatus inStatus, std::string inMessage)
            : Status(inStatus), Message(std::move(inMessage))
        {
        }
        ProjectOpenStatus Status = ProjectOpenStatus::InvalidDescriptor;
        std::string Message;

        bool IsSuccess() const
        {
            return Status == ProjectOpenStatus::Success;
        }
    };

    struct ProjectContext
    {
        ProjectDescriptor Descriptor;
        ProjectSettings Settings;

        void Reset()
        {
            Descriptor = ProjectDescriptor{};
            Settings = ProjectSettings{};
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

        const ProjectContext& GetCurrentProjectCtx() { return m_CurrentProjectCtx; }
        ProjectOpenResult OpenProject(const std::filesystem::path& projectRoot);    // TODO: currently we only accept absolute path, e.g. C:/Projects/MyProject, but maybe we should also support relative path like ./MyProject or MyProject, and we can resolve them to absolute path internally
        void CloseCurrentProject();

    private:
        bool LoadProjectDesc(const std::filesystem::path& descriptorPath, ProjectDescriptor& outDescriptor);
        bool LoadProjectSettings(const std::filesystem::path& settingsPath, ProjectSettings& outSettings);


    private:
        static constexpr const char* kMEProjectExtension = ".meproject";
        static constexpr const char* kMEProjectSettingsExtension = ".mesettings";
        ProjectContext m_CurrentProjectCtx;
    };
}

#pragma once

#include "Core.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/SceneRendererKind.h"

#include <filesystem>
#include <optional>
#include <string>

namespace minEngine
{
    enum class ApplicationMode
    {
        Editor,
        Test,
    };

    enum class TestRunKind
    {
        Smoke,
        Full,
        SingleSuite,
    };

    struct CommandLineResult
    {
        ApplicationMode Mode = ApplicationMode::Editor;

        std::optional<std::filesystem::path> EngineConfigPath;
        std::optional<std::filesystem::path> EngineRootOverride;

        std::optional<std::filesystem::path> ProjectDescriptorPath;

        /** Default OpenGL; set by global `--rhi opengl|vulkan` (aliases gl|vk). */
        RHIBackendType RHIBackend = RHIBackendType::OpenGL;

        /** Default ForwardRenderer; `--renderer manual` selects diagnostic path. */
        SceneRendererKind SceneRenderer = SceneRendererKind::Forward;

        /**
         * Optional `--frame-threads single|multi`. When unset, env MINENGINE_FRAME_THREADS
         * then default Single (see FrameThreading::ResolveMode).
         */
        std::optional<FrameThreadMode> FrameThreads;

        TestRunKind TestKind = TestRunKind::Smoke;
        std::string SuiteId;
        std::optional<std::string> ReflectionSuiteFilter;

        /** Editor automation: Named Pipe name for Host. */
        std::optional<std::string> AutomationPipeName;
        /** Main ready channel path; written by Host after Listen (not by CLI parse). */
        std::optional<std::filesystem::path> AutomationReadyFilePath;
        /** Project-relative scene asset path (e.g. Scenes/test.mescene). Guid deferred. */
        std::optional<std::string> OpenSceneAssetPath;

        bool RequestedHelp = false;
        bool RequestedVersion = false;
    };
}

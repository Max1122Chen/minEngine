#include "EditorHostGameModule.h"

#include "Runtime/Core/Log/LogSystem.h"

namespace minEngine
{
    namespace
    {
        // A string LITERAL pointer, constant-initialized: no dynamic initialization order and no
        // namespace-scope object that could run before the reflection registry exists
        // (BUILD-F04 F3). The host always passes a literal (see main.h), so the pointer stays valid
        // for the process lifetime.
        const char* s_HostEditorGameModule = kEditorNoGameModule;
    }

    void SetHostEditorGameModule(const char* moduleId)
    {
        s_HostEditorGameModule = (moduleId != nullptr && moduleId[0] != '\0')
            ? moduleId
            : kEditorNoGameModule;
    }

    std::string_view GetHostEditorGameModule()
    {
        return s_HostEditorGameModule;
    }

    bool HostEditorIsAssetOnly()
    {
        return TrimEditorModuleId(s_HostEditorGameModule) == kEditorNoGameModule;
    }

    EditorModuleGateResult CheckProjectModuleRequirement(
        const std::string& projectName,
        const std::string& projectRequiredModule)
    {
        const std::string_view hostModule = GetHostEditorGameModule();
        const std::string_view required = TrimEditorModuleId(projectRequiredModule);

        switch (EvaluateEditorModuleGate(projectRequiredModule, hostModule))
        {
        case EditorModuleGateVerdict::Pass:
            return EditorModuleGateResult{};

        case EditorModuleGateVerdict::PassAssetOnlyNotice:
            // Allowed, but never silent: an asset-only editor opening an asset-only project is the
            // supported product shape, and the operator should still be told what this binary is.
            ME_LOG(LogEditor, Warn,
                "Editor: project '{}' declares no RequiredGameModule; this editor was built WITHOUT "
                "a game C++ module (asset-only), so only asset-only projects are supported here.",
                projectName);
            return EditorModuleGateResult{};

        case EditorModuleGateVerdict::FailNoModule:
            ME_LOG(LogEditor, Error,
                "Editor: refusing to open project '{}': it declares RequiredGameModule '{}'.",
                projectName,
                std::string(required));
            ME_LOG(LogEditor, Error,
                "Editor: this editor host was built with no game C++ module (identity '{}'), so not "
                "one of the project's C++ component types could be registered - the project would "
                "open looking like its components were lost.",
                std::string(TrimEditorModuleId(hostModule)));
            break;

        case EditorModuleGateVerdict::FailMismatch:
            ME_LOG(LogEditor, Error,
                "Editor: refusing to open project '{}': it declares RequiredGameModule '{}', but this "
                "editor host was built with '{}'.",
                projectName,
                std::string(required),
                std::string(TrimEditorModuleId(hostModule)));
            ME_LOG(LogEditor, Error,
                "Editor: only the module the project declares can register its C++ component types; "
                "a different module registers a different set and the project would open incomplete.");
            break;
        }

        // Recovery guidance, identical for both failure verdicts - this is the paragraph the user
        // is missing today when their components "disappear".
        ME_LOG(LogEditor, Error,
            "Editor: recovery - build a project editor that links this module: "
            "minengine_add_editor_executable(<Project>Editor MAIN_SOURCE <main.cpp> GAME_MODULE {}), "
            "or run the editor shipped with the project: <Project>Editor.exe. "
            "MaximumEditor.exe is the asset-only editor and cannot load game C++ modules.",
            std::string(required));
        ME_LOG(LogEditor, Error,
            "Editor: exiting with code {} (hard error, no silent downgrade - BUILD-F04 S05 / Q4).",
            kEditorExitCodeModuleRequirementUnmet);

        EditorModuleGateResult result;
        result.AllowProject = false;
        result.ExitCode = kEditorExitCodeModuleRequirementUnmet;
        return result;
    }
}

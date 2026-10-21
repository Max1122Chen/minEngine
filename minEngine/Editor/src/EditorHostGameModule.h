#pragma once

#include "EditorAPI.h"

#include <string>
#include <string_view>

namespace minEngine
{
    // Identity of an editor host that links NO game C++ module.
    inline constexpr const char* kEditorNoGameModule = "(none)";

    // The game module this EDITOR HOST was compiled against. libMaximumEditord.dll is one library
    // shared by MaximumEditor.exe, SampleEditor.exe and the external project editors, so the editor
    // library cannot know this at compile time - the host states it through main.h before
    // CreateApplication() (MINENGINE_EDITOR_GAME_MODULE / MINENGINE_EDITOR_ASSET_ONLY, both injected
    // by minengine_add_editor_executable). A host that never calls the setter reads as "(none)".
    //
    // Only the setter is exported: the library and the automation harness consume the getter, and
    // the editor DLL's export surface is a contract, not a dumping ground.
    EDITOR_API void SetHostEditorGameModule(const char* moduleId);
    std::string_view GetHostEditorGameModule();
    bool HostEditorIsAssetOnly();

    enum class EditorModuleGateVerdict
    {
        Pass,                 // the project declares nothing, or exactly this editor's module
        PassAssetOnlyNotice,  // the project declares nothing; this editor has no game module
        FailNoModule,         // the project declares M; this editor was built with no module
        FailMismatch,         // the project declares M; this editor was built with N != M
    };

    // Pure verdict - no logging, no state. It is separated from the reporting path so the whole
    // behaviour matrix is one readable function.
    // "(none)" and whitespace-only declarations both mean "no requirement": a descriptor that omits
    // the field deserializes to "" and must keep behaving exactly like a descriptor that omits it.
    inline std::string_view TrimEditorModuleId(std::string_view value)
    {
        const char* kWhitespace = " \t\r\n";
        const std::size_t first = value.find_first_not_of(kWhitespace);
        if (first == std::string_view::npos)
        {
            return {};
        }
        const std::size_t last = value.find_last_not_of(kWhitespace);
        return value.substr(first, last - first + 1);
    }

    inline EditorModuleGateVerdict EvaluateEditorModuleGate(
        std::string_view projectRequiredModule,
        std::string_view editorGameModule)
    {
        std::string_view required = TrimEditorModuleId(projectRequiredModule);
        const std::string_view host = TrimEditorModuleId(editorGameModule);
        if (required == kEditorNoGameModule)
        {
            required = std::string_view{};
        }
        const bool editorHasModule = !host.empty() && host != kEditorNoGameModule;
        if (required.empty())
        {
            return editorHasModule ? EditorModuleGateVerdict::Pass
                                   : EditorModuleGateVerdict::PassAssetOnlyNotice;
        }
        if (!editorHasModule)
        {
            return EditorModuleGateVerdict::FailNoModule;
        }
        return required == host ? EditorModuleGateVerdict::Pass
                                : EditorModuleGateVerdict::FailMismatch;
    }

    // Non-zero by construction, and a hard error rather than a silent downgrade,
    // and distinct from CommandLineExitCode's 1/2 so a caller can tell "wrong editor" from
    // "bad command line" without parsing logs.
    inline constexpr int kEditorExitCodeModuleRequirementUnmet = 3;

    struct EditorModuleGateResult
    {
        bool AllowProject = true;
        int ExitCode = 0;
    };

    // Startup / open-project gate. Logs the full recovery guidance on failure and tells the caller
    // whether the project may be opened at all.
    EditorModuleGateResult CheckProjectModuleRequirement(
        const std::string& projectName,
        const std::string& projectRequiredModule);
}
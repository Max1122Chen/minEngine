#include "Shell/Document/EditorDocumentTypes.h"

#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/IEditorContext.h"

#include "SubEditor/Scene/SceneEditor.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Resource/AssetMeta.h"

#include <filesystem>

namespace minEngine
{
    namespace
    {
        std::string FileTitleFromPath(const std::string& path)
        {
            return std::filesystem::path(path).filename().string();
        }

        SceneEditor* GetSceneEditor(IEditorContext& context)
        {
            return dynamic_cast<SceneEditor*>(context.FindSubModule(SceneEditor::kModuleId));
        }

        MaterialEditor* GetMaterialEditor(IEditorContext& context)
        {
            return dynamic_cast<MaterialEditor*>(context.FindSubModule(MaterialEditor::kModuleId));
        }

        AnimationGraphEditor* GetAnimGraphEditor(IEditorContext& context)
        {
            return dynamic_cast<AnimationGraphEditor*>(context.FindSubModule(AnimationGraphEditor::kModuleId));
        }
    }

    void RegisterBuiltinEditorDocumentTypes(IEditorContext& context, EditorDocumentHost& host)
    {
        (void)context;
        EditorDocumentTypeRegistry& registry = host.GetTypeRegistry();

        {
            EditorDocumentTypeInfo scene;
            scene.TypeId = "Scene";
            scene.DisplayName = "Scene";
            scene.ModuleId = SceneEditor::kModuleId;
            scene.AllowMultipleSessions = false;
            scene.AllowCloseLastOfType = false;
            scene.CanOpenAsset = [](const AssetMeta& meta) { return meta.AssetType == "Scene"; };
            scene.MakeAssetKey = [](const AssetMeta& meta) { return meta.AssetPath; };
            scene.MakeTitle = [](const AssetMeta& meta) { return FileTitleFromPath(meta.AssetPath); };
            scene.BindSession = [](IEditorContext& ctx, const AssetMeta& meta, EditorDocumentSession& session)
            {
                SceneEditor* editor = GetSceneEditor(ctx);
                if (editor == nullptr)
                {
                    return false;
                }
                if (!editor->OpenAsset(meta))
                {
                    return false;
                }
                session.SetTitle(FileTitleFromPath(meta.AssetPath));
                return true;
            };
            scene.ActivateSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                SceneEditor* editor = GetSceneEditor(ctx);
                if (editor == nullptr)
                {
                    return false;
                }
                editor->ExitPrefabStage();
                if (!session.GetAssetKey().empty()
                    && editor->GetOpenedSceneAssetPath() != session.GetAssetKey())
                {
                    return editor->OpenSceneByPath(ctx, session.GetAssetKey());
                }
                return true;
            };
            scene.CloseSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                (void)ctx;
                (void)session;
            };
            scene.QueryDirty = [](IEditorContext& ctx, const EditorDocumentSession& session)
            {
                (void)session;
                const SceneEditor* editor = GetSceneEditor(ctx);
                return editor != nullptr && editor->IsSceneDirty();
            };
            scene.SaveSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                (void)session;
                SceneEditor* editor = GetSceneEditor(ctx);
                return editor != nullptr && editor->SaveCurrentScene(ctx);
            };
            registry.Register(std::move(scene));
        }

        {
            EditorDocumentTypeInfo material;
            material.TypeId = "Material";
            material.DisplayName = "Material";
            material.ModuleId = MaterialEditor::kModuleId;
            material.AllowMultipleSessions = true;
            material.AllowCloseLastOfType = true;
            material.CanOpenAsset = [](const AssetMeta& meta) { return meta.AssetType == "Material"; };
            material.MakeAssetKey = [](const AssetMeta& meta) { return meta.AssetPath; };
            material.MakeTitle = [](const AssetMeta& meta) { return FileTitleFromPath(meta.AssetPath); };
            material.BindSession = [](IEditorContext& ctx, const AssetMeta& meta, EditorDocumentSession& session)
            {
                MaterialEditor* editor = GetMaterialEditor(ctx);
                if (editor == nullptr)
                {
                    return false;
                }
                if (!editor->OpenAsset(meta))
                {
                    return false;
                }
                session.SetTitle(FileTitleFromPath(meta.AssetPath));
                return true;
            };
            material.ActivateSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                MaterialEditor* editor = GetMaterialEditor(ctx);
                return editor != nullptr && editor->ActivateStoredSession(session.GetAssetKey());
            };
            material.CloseSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                if (MaterialEditor* editor = GetMaterialEditor(ctx))
                {
                    editor->DiscardStoredSession(session.GetAssetKey());
                }
            };
            material.QueryDirty = [](IEditorContext& ctx, const EditorDocumentSession& session)
            {
                const MaterialEditor* editor = GetMaterialEditor(ctx);
                return editor != nullptr && editor->IsStoredSessionDirty(session.GetAssetKey());
            };
            material.SaveSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                MaterialEditor* editor = GetMaterialEditor(ctx);
                if (editor == nullptr)
                {
                    return false;
                }
                if (!editor->ActivateStoredSession(session.GetAssetKey()))
                {
                    return false;
                }
                return editor->SaveActiveMaterial();
            };
            registry.Register(std::move(material));
        }

        {
            EditorDocumentTypeInfo animGraph;
            animGraph.TypeId = "AnimationGraph";
            animGraph.DisplayName = "Animation Graph";
            animGraph.ModuleId = AnimationGraphEditor::kModuleId;
            animGraph.AllowMultipleSessions = true;
            animGraph.AllowCloseLastOfType = true;
            animGraph.CanOpenAsset = [](const AssetMeta& meta) { return meta.AssetType == "AnimationGraph"; };
            animGraph.MakeAssetKey = [](const AssetMeta& meta) { return meta.AssetPath; };
            animGraph.MakeTitle = [](const AssetMeta& meta) { return FileTitleFromPath(meta.AssetPath); };
            animGraph.BindSession = [](IEditorContext& ctx, const AssetMeta& meta, EditorDocumentSession& session)
            {
                AnimationGraphEditor* editor = GetAnimGraphEditor(ctx);
                if (editor == nullptr)
                {
                    return false;
                }
                if (!editor->OpenAsset(meta))
                {
                    return false;
                }
                session.SetTitle(FileTitleFromPath(meta.AssetPath));
                return true;
            };
            animGraph.ActivateSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                AnimationGraphEditor* editor = GetAnimGraphEditor(ctx);
                return editor != nullptr && editor->ActivateStoredSession(session.GetAssetKey());
            };
            animGraph.CloseSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                if (AnimationGraphEditor* editor = GetAnimGraphEditor(ctx))
                {
                    editor->DiscardStoredSession(session.GetAssetKey());
                }
            };
            animGraph.QueryDirty = [](IEditorContext& ctx, const EditorDocumentSession& session)
            {
                const AnimationGraphEditor* editor = GetAnimGraphEditor(ctx);
                return editor != nullptr && editor->IsStoredSessionDirty(session.GetAssetKey());
            };
            animGraph.SaveSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                AnimationGraphEditor* editor = GetAnimGraphEditor(ctx);
                if (editor == nullptr)
                {
                    return false;
                }
                if (!editor->ActivateStoredSession(session.GetAssetKey()))
                {
                    return false;
                }
                return editor->SaveActiveGraph();
            };
            registry.Register(std::move(animGraph));
        }

        {
            // Prefab documents reuse SceneEditor UI (Hierarchy / Viewport / Inspector).
            EditorDocumentTypeInfo prefab;
            prefab.TypeId = "Prefab";
            prefab.DisplayName = "Prefab";
            prefab.ModuleId = SceneEditor::kModuleId;
            prefab.AllowMultipleSessions = true;
            prefab.AllowCloseLastOfType = true;
            prefab.CanOpenAsset = [](const AssetMeta& meta) { return meta.AssetType == "Prefab"; };
            prefab.MakeAssetKey = [](const AssetMeta& meta) { return meta.AssetPath; };
            prefab.MakeTitle = [](const AssetMeta& meta) { return FileTitleFromPath(meta.AssetPath); };
            prefab.BindSession = [](IEditorContext& ctx, const AssetMeta& meta, EditorDocumentSession& session)
            {
                SceneEditor* editor = GetSceneEditor(ctx);
                if (editor == nullptr)
                {
                    return false;
                }

                std::string error;
                if (!editor->GetPrefabStages().OpenFromAsset(meta, &error))
                {
                    ME_LOG(LogEditor, Error, "Open Prefab document failed: {}", error);
                    return false;
                }

                session.SetTitle(FileTitleFromPath(meta.AssetPath));
                return true;
            };
            prefab.ActivateSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                SceneEditor* editor = GetSceneEditor(ctx);
                return editor != nullptr && editor->EnterPrefabStage(session.GetAssetKey());
            };
            prefab.CloseSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                SceneEditor* editor = GetSceneEditor(ctx);
                if (editor == nullptr)
                {
                    return;
                }

                if (editor->GetPrefabStages().GetActiveAssetKey() == session.GetAssetKey())
                {
                    editor->ExitPrefabStage();
                }
                editor->GetPrefabStages().Discard(session.GetAssetKey());
            };
            prefab.QueryDirty = [](IEditorContext& ctx, const EditorDocumentSession& session)
            {
                const SceneEditor* editor = GetSceneEditor(ctx);
                return editor != nullptr && editor->GetPrefabStages().IsDirty(session.GetAssetKey());
            };
            prefab.SaveSession = [](IEditorContext& ctx, EditorDocumentSession& session)
            {
                SceneEditor* editor = GetSceneEditor(ctx);
                if (editor == nullptr)
                {
                    return false;
                }
                if (!editor->EnterPrefabStage(session.GetAssetKey()))
                {
                    return false;
                }

                std::string error;
                return editor->GetPrefabStages().SaveActive(ctx, &error);
            };
            registry.Register(std::move(prefab));
        }
    }
}

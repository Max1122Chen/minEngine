#pragma once

#include "SubEditor/Prefab/PrefabStage.h"

#include <string>
#include <unordered_map>

namespace minEngine
{
    class AssetMeta;
    class IEditorContext;

    /**
     * Owns Prefab Stage Scenes keyed by asset path.
     * SceneEditor binds InspectingScene to the active stage while a Prefab document is focused.
     * Dirty is per-stage; Undo uses the Prefab EditorDocumentSession CommandStack.
     */
    class PrefabStageController
    {
    public:
        bool OpenFromAsset(const AssetMeta& meta, std::string* outError = nullptr);
        bool Activate(const std::string& assetKey, IEditorContext& context);
        void Discard(const std::string& assetKey);
        void ExitActive(IEditorContext& context);

        bool HasStage(const std::string& assetKey) const;
        bool IsDirty(const std::string& assetKey) const;
        void MarkDirty(const std::string& assetKey);
        void ClearDirty(const std::string& assetKey);

        PrefabEditorStage* FindStage(const std::string& assetKey);
        const PrefabEditorStage* FindStage(const std::string& assetKey) const;
        PrefabEditorStage* GetActiveStage();
        const PrefabEditorStage* GetActiveStage() const;
        const std::string& GetActiveAssetKey() const { return m_ActiveAssetKey; }
        bool HasActiveStage() const { return !m_ActiveAssetKey.empty(); }

        /** Write the active Stage tree back to its Prefab asset (asset Guid preserved). */
        bool SaveActive(IEditorContext& context, std::string* outError = nullptr);

        /** Same as SaveActive for a named Stage, so automation can save without stealing focus. */
        bool SaveStage(const std::string& assetKey, IEditorContext& context, std::string* outError = nullptr);

        /** Drop all stage Scene strong refs (Editor shutdown before Engine). */
        void Shutdown();

    private:
        bool BuildStage(const AssetMeta& meta, PrefabEditorStage& outStage, std::string* outError);
        void DestroyStage(PrefabEditorStage& stage);

        std::unordered_map<std::string, PrefabEditorStage> m_StagesByKey;
        std::string m_ActiveAssetKey;
    };
}

#include "Commands/Scene/DeleteGameObjectCommand.h"

#include "Commands/Scene/EditorObjectSnapshot.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

#include <limits>

namespace minEngine
{
    DeleteGameObjectCommand::DeleteGameObjectCommand(SceneEditor& sceneEditor, uint64_t gameObjectId)
        : m_SceneEditor(sceneEditor)
        , m_GameObjectId(gameObjectId)
    {
        m_Description = "Delete GameObject";
    }

    void DeleteGameObjectCommand::Execute()
    {
        m_Removed = false;
        m_SnapshotEnvelope.clear();

        EditorObjectSnapshot snapshot;
        std::string description;
        if (!m_SceneEditor.TryCaptureGameObjectSnapshotForDelete(m_GameObjectId, snapshot, description))
        {
            return;
        }

        if (!description.empty())
        {
            m_Description = std::move(description);
        }

        if (!EditorObjectSnapshotUtil::WriteEnvelope(snapshot, m_SnapshotEnvelope))
        {
            ME_CORE_ERROR("DeleteGameObjectCommand: failed to write snapshot envelope.");
            return;
        }

        std::string discardedName;
        Transform discardedTransform;
        if (!m_SceneEditor.ApplyRemoveGameObjectFromScene(m_GameObjectId, discardedName, discardedTransform))
        {
            m_SnapshotEnvelope.clear();
            return;
        }

        m_Removed = true;
    }

    void DeleteGameObjectCommand::Undo()
    {
        if (!m_Removed || m_SnapshotEnvelope.empty())
        {
            return;
        }

        EditorObjectSnapshot snapshot;
        if (!EditorObjectSnapshotUtil::ReadEnvelope(m_SnapshotEnvelope, snapshot))
        {
            ME_CORE_ERROR("DeleteGameObjectCommand: failed to read snapshot envelope.");
            return;
        }

        const uint64_t restoredId = m_SceneEditor.ApplyRestoreGameObjectFromSnapshot(snapshot);
        if (restoredId == std::numeric_limits<uint64_t>::max())
        {
            return;
        }

        m_GameObjectId = restoredId;
        m_SceneEditor.SelectGameObject(m_GameObjectId);
    }

    const char* DeleteGameObjectCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

#include "Commands/Scene/RemoveComponentCommand.h"

#include "Commands/Scene/EditorObjectSnapshot.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/Component.h"

namespace minEngine
{
    RemoveComponentCommand::RemoveComponentCommand(SceneEditor& sceneEditor,
                                                   uint64_t ownerGameObjectId,
                                                   const Component& targetComponent)
        : m_SceneEditor(sceneEditor)
        , m_OwnerGameObjectId(ownerGameObjectId)
        , m_ComponentGuidHigh(targetComponent.GetGuid().High)
        , m_ComponentGuidLow(targetComponent.GetGuid().Low)
        , m_ComponentIndex(-1)
    {
        if (const Reflection::MEClass* classInfo = targetComponent.GetClass())
        {
            m_ComponentTypeName = classInfo->GetName();
        }

        m_Description = "Remove Component";
    }

    void RemoveComponentCommand::Execute()
    {
        m_Removed = false;
        m_SnapshotEnvelope.clear();

        EditorObjectSnapshot snapshot;
        std::string description;
        const GUID componentGuid(m_ComponentGuidHigh, m_ComponentGuidLow);
        if (!m_SceneEditor.TryCaptureComponentSnapshotForRemove(
                m_OwnerGameObjectId,
                componentGuid,
                snapshot,
                m_ComponentIndex,
                description))
        {
            return;
        }

        if (!description.empty())
        {
            m_Description = std::move(description);
        }

        if (!EditorObjectSnapshotUtil::WriteEnvelope(snapshot, m_SnapshotEnvelope))
        {
            ME_CORE_ERROR("RemoveComponentCommand: failed to write snapshot envelope.");
            return;
        }

        if (!m_SceneEditor.ApplyRemoveComponentByGuid(m_OwnerGameObjectId, componentGuid))
        {
            m_SnapshotEnvelope.clear();
            return;
        }

        m_Removed = true;
    }

    void RemoveComponentCommand::Undo()
    {
        if (!m_Removed || m_SnapshotEnvelope.empty())
        {
            return;
        }

        EditorObjectSnapshot snapshot;
        if (!EditorObjectSnapshotUtil::ReadEnvelope(m_SnapshotEnvelope, snapshot))
        {
            ME_CORE_ERROR("RemoveComponentCommand: failed to read snapshot envelope.");
            return;
        }

        snapshot.componentIndexInOwner = m_ComponentIndex;

        if (m_SceneEditor.ApplyRestoreComponentFromSnapshot(m_OwnerGameObjectId, snapshot) == nullptr)
        {
            return;
        }

        m_Removed = false;
    }

    const char* RemoveComponentCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

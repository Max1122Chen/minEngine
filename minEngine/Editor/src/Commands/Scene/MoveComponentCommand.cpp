#include "Commands/Scene/MoveComponentCommand.h"

#include "Runtime/Core/GUID/GUID.h"
#include "SubEditor/Scene/SceneEditor.h"

namespace minEngine
{
    MoveComponentCommand::MoveComponentCommand(SceneEditor& sceneEditor,
                                               uint64_t ownerGameObjectId,
                                               const GUID& componentGuid,
                                               size_t fromIndex,
                                               size_t toIndex)
        : m_SceneEditor(sceneEditor)
        , m_OwnerGameObjectId(ownerGameObjectId)
        , m_ComponentGuidHigh(componentGuid.High)
        , m_ComponentGuidLow(componentGuid.Low)
        , m_FromIndex(fromIndex)
        , m_ToIndex(toIndex)
    {
        m_Description = "Reorder Component";
    }

    void MoveComponentCommand::Execute()
    {
        m_SceneEditor.ApplyMoveComponent(
            m_OwnerGameObjectId, GUID(m_ComponentGuidHigh, m_ComponentGuidLow), m_ToIndex);
    }

    void MoveComponentCommand::Undo()
    {
        m_SceneEditor.ApplyMoveComponent(
            m_OwnerGameObjectId, GUID(m_ComponentGuidHigh, m_ComponentGuidLow), m_FromIndex);
    }

    const char* MoveComponentCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

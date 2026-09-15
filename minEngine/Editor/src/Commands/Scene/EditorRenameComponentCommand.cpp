#include "Commands/Scene/EditorRenameComponentCommand.h"

#include "Runtime/Core/GUID/GUID.h"
#include "SubEditor/Scene/SceneEditor.h"

namespace minEngine
{
    EditorRenameComponentCommand::EditorRenameComponentCommand(SceneEditor& sceneEditor,
                                                   uint64_t ownerGameObjectId,
                                                   const GUID& componentGuid,
                                                   std::string oldName,
                                                   std::string newName)
        : m_SceneEditor(sceneEditor)
        , m_OwnerGameObjectId(ownerGameObjectId)
        , m_ComponentGuidHigh(componentGuid.High)
        , m_ComponentGuidLow(componentGuid.Low)
        , m_OldName(std::move(oldName))
        , m_NewName(std::move(newName))
    {
        m_Description = "Rename Component";
    }

    void EditorRenameComponentCommand::Execute()
    {
        m_SceneEditor.ApplyRenameComponent(
            m_OwnerGameObjectId, GUID(m_ComponentGuidHigh, m_ComponentGuidLow), m_NewName);
    }

    void EditorRenameComponentCommand::Undo()
    {
        m_SceneEditor.ApplyRenameComponent(
            m_OwnerGameObjectId, GUID(m_ComponentGuidHigh, m_ComponentGuidLow), m_OldName);
    }

    const char* EditorRenameComponentCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

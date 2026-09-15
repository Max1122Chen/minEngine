#include "Commands/Material/EditorAddMaterialNodeCommand.h"

#include "SubEditor/Material/MaterialEditor.h"

#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    EditorAddMaterialNodeCommand::EditorAddMaterialNodeCommand(MaterialEditor& materialEditor,
                                                               std::string nodeDefClassName,
                                                               float editorPosX,
                                                               float editorPosY)
        : m_MaterialEditor(materialEditor)
        , m_NodeDefClassName(std::move(nodeDefClassName))
        , m_EditorPosX(editorPosX)
        , m_EditorPosY(editorPosY)
    {
        m_Description = "Add Material Node";
    }

    void EditorAddMaterialNodeCommand::Execute()
    {
        GUID createdGuid;
        if (!m_MaterialEditor.ApplyAddNode(m_NodeDefClassName, m_EditorPosX, m_EditorPosY, &createdGuid))
        {
            m_HasCreatedNode = false;
            return;
        }

        m_CreatedNodeDefHigh = createdGuid.High;
        m_CreatedNodeDefLow = createdGuid.Low;
        m_HasCreatedNode = true;
    }

    void EditorAddMaterialNodeCommand::Undo()
    {
        if (!m_HasCreatedNode)
        {
            return;
        }

        if (m_MaterialEditor.ApplyRemoveNodeByGuid(GUID(m_CreatedNodeDefHigh, m_CreatedNodeDefLow)))
        {
            m_HasCreatedNode = false;
        }
    }

    const char* EditorAddMaterialNodeCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

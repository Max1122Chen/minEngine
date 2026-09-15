#include "Commands/Material/EditorRemoveMaterialNodeCommand.h"

#include "SubEditor/Material/MaterialEditor.h"

#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    EditorRemoveMaterialNodeCommand::EditorRemoveMaterialNodeCommand(MaterialEditor& materialEditor,
                                                                     uint64_t nodeDefHigh,
                                                                     uint64_t nodeDefLow,
                                                                     std::vector<uint8_t> nodeSnapshot,
                                                                     std::vector<MaterialNodeInboundLink> inboundLinks)
        : m_MaterialEditor(materialEditor)
        , m_NodeDefHigh(nodeDefHigh)
        , m_NodeDefLow(nodeDefLow)
        , m_NodeSnapshot(std::move(nodeSnapshot))
        , m_InboundLinks(std::move(inboundLinks))
    {
        m_Description = "Remove Material Node";
    }

    void EditorRemoveMaterialNodeCommand::Execute()
    {
        m_Removed = m_MaterialEditor.ApplyRemoveNodeByGuid(GUID(m_NodeDefHigh, m_NodeDefLow));
    }

    void EditorRemoveMaterialNodeCommand::Undo()
    {
        if (!m_Removed)
        {
            return;
        }

        if (m_MaterialEditor.ApplyRestoreNodeFromSnapshot(m_NodeSnapshot, m_InboundLinks))
        {
            m_Removed = false;
        }
    }

    const char* EditorRemoveMaterialNodeCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

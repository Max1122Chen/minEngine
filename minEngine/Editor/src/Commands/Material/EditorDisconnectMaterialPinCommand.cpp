#include "Commands/Material/EditorDisconnectMaterialPinCommand.h"

#include "SubEditor/Material/MaterialEditor.h"

#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    EditorDisconnectMaterialPinCommand::EditorDisconnectMaterialPinCommand(MaterialEditor& materialEditor,
                                                                           uint64_t toNodeDefHigh,
                                                                           uint64_t toNodeDefLow,
                                                                           int32_t toInputIndex,
                                                                           uint64_t fromNodeDefHigh,
                                                                           uint64_t fromNodeDefLow,
                                                                           int32_t fromOutputIndex)
        : m_MaterialEditor(materialEditor)
        , m_ToNodeDefHigh(toNodeDefHigh)
        , m_ToNodeDefLow(toNodeDefLow)
        , m_ToInputIndex(toInputIndex)
        , m_FromNodeDefHigh(fromNodeDefHigh)
        , m_FromNodeDefLow(fromNodeDefLow)
        , m_FromOutputIndex(fromOutputIndex)
    {
        m_Description = "Disconnect Material Pin";
    }

    void EditorDisconnectMaterialPinCommand::Execute()
    {
        m_MaterialEditor.ApplyDisconnectInput(GUID(m_ToNodeDefHigh, m_ToNodeDefLow), m_ToInputIndex);
    }

    void EditorDisconnectMaterialPinCommand::Undo()
    {
        m_MaterialEditor.ApplyConnectPins(
            GUID(m_FromNodeDefHigh, m_FromNodeDefLow),
            m_FromOutputIndex,
            GUID(m_ToNodeDefHigh, m_ToNodeDefLow),
            m_ToInputIndex);
    }

    const char* EditorDisconnectMaterialPinCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

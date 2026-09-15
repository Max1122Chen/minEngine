#include "Commands/Material/EditorConnectMaterialPinsCommand.h"

#include "SubEditor/Material/MaterialEditor.h"

#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    EditorConnectMaterialPinsCommand::EditorConnectMaterialPinsCommand(MaterialEditor& materialEditor,
                                                                       uint64_t fromNodeDefHigh,
                                                                       uint64_t fromNodeDefLow,
                                                                       int32_t fromOutputIndex,
                                                                       uint64_t toNodeDefHigh,
                                                                       uint64_t toNodeDefLow,
                                                                       int32_t toInputIndex,
                                                                       uint64_t previousFromNodeDefHigh,
                                                                       uint64_t previousFromNodeDefLow,
                                                                       int32_t previousFromOutputIndex,
                                                                       bool hadPreviousConnection)
        : m_MaterialEditor(materialEditor)
        , m_FromNodeDefHigh(fromNodeDefHigh)
        , m_FromNodeDefLow(fromNodeDefLow)
        , m_FromOutputIndex(fromOutputIndex)
        , m_ToNodeDefHigh(toNodeDefHigh)
        , m_ToNodeDefLow(toNodeDefLow)
        , m_ToInputIndex(toInputIndex)
        , m_PreviousFromNodeDefHigh(previousFromNodeDefHigh)
        , m_PreviousFromNodeDefLow(previousFromNodeDefLow)
        , m_PreviousFromOutputIndex(previousFromOutputIndex)
        , m_HadPreviousConnection(hadPreviousConnection)
    {
        m_Description = "Connect Material Pins";
    }

    void EditorConnectMaterialPinsCommand::Execute()
    {
        m_MaterialEditor.ApplyConnectPins(
            GUID(m_FromNodeDefHigh, m_FromNodeDefLow),
            m_FromOutputIndex,
            GUID(m_ToNodeDefHigh, m_ToNodeDefLow),
            m_ToInputIndex);
    }

    void EditorConnectMaterialPinsCommand::Undo()
    {
        m_MaterialEditor.ApplyDisconnectInput(GUID(m_ToNodeDefHigh, m_ToNodeDefLow), m_ToInputIndex);
        if (m_HadPreviousConnection)
        {
            m_MaterialEditor.ApplyConnectPins(
                GUID(m_PreviousFromNodeDefHigh, m_PreviousFromNodeDefLow),
                m_PreviousFromOutputIndex,
                GUID(m_ToNodeDefHigh, m_ToNodeDefLow),
                m_ToInputIndex);
        }
    }

    const char* EditorConnectMaterialPinsCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

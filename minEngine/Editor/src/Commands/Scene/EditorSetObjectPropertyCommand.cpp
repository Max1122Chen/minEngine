#include "Commands/Scene/EditorSetObjectPropertyCommand.h"

#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    EditorSetObjectPropertyCommand::EditorSetObjectPropertyCommand(EditorSetObjectPropertyTarget& target,
                                                                   const GUID& ownerGuid,
                                                                   std::string ownerClassName,
                                                                   std::string propertyPath,
                                                                   std::vector<uint8_t> beforeValue,
                                                                   std::vector<uint8_t> afterValue,
                                                                   bool applyOnFirstExecute,
                                                                   EditorSetObjectPropertySideEffects sideEffects)
        : m_Target(target)
        , m_OwnerGuidHigh(ownerGuid.High)
        , m_OwnerGuidLow(ownerGuid.Low)
        , m_OwnerClassName(std::move(ownerClassName))
        , m_PropertyPath(std::move(propertyPath))
        , m_BeforeValue(std::move(beforeValue))
        , m_AfterValue(std::move(afterValue))
        , m_SideEffects(std::move(sideEffects))
        , m_ApplyOnNextExecute(applyOnFirstExecute)
    {
        m_Description = "Set " + m_PropertyPath;
    }

    void EditorSetObjectPropertyCommand::Execute()
    {
        if (m_ApplyOnNextExecute)
        {
            const GUID ownerGuid(m_OwnerGuidHigh, m_OwnerGuidLow);
            m_Target.ApplySetObjectProperty(ownerGuid, m_OwnerClassName, m_PropertyPath, m_AfterValue);
            if (m_SideEffects.AfterExecute)
            {
                m_SideEffects.AfterExecute();
            }
        }
        m_ApplyOnNextExecute = true;
    }

    void EditorSetObjectPropertyCommand::Undo()
    {
        const GUID ownerGuid(m_OwnerGuidHigh, m_OwnerGuidLow);
        m_Target.ApplySetObjectProperty(ownerGuid, m_OwnerClassName, m_PropertyPath, m_BeforeValue);
        if (m_SideEffects.AfterUndo)
        {
            m_SideEffects.AfterUndo();
        }
    }

    const char* EditorSetObjectPropertyCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

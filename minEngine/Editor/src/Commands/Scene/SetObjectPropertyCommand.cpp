#include "Commands/Scene/SetObjectPropertyCommand.h"

#include "Scene/SceneEditor.h"

#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    SetObjectPropertyCommand::SetObjectPropertyCommand(SceneEditor& sceneEditor,
                                                       const GUID& ownerGuid,
                                                       std::string ownerClassName,
                                                       std::string propertyName,
                                                       std::vector<uint8_t> beforeValue,
                                                       std::vector<uint8_t> afterValue)
        : m_SceneEditor(sceneEditor)
        , m_OwnerGuidHigh(ownerGuid.High)
        , m_OwnerGuidLow(ownerGuid.Low)
        , m_OwnerClassName(std::move(ownerClassName))
        , m_PropertyName(std::move(propertyName))
        , m_BeforeValue(std::move(beforeValue))
        , m_AfterValue(std::move(afterValue))
    {
        m_Description = "Set " + m_PropertyName;
    }

    void SetObjectPropertyCommand::Execute()
    {
        const GUID ownerGuid(m_OwnerGuidHigh, m_OwnerGuidLow);
        m_SceneEditor.ApplySetObjectProperty(ownerGuid, m_OwnerClassName, m_PropertyName, m_AfterValue);
    }

    void SetObjectPropertyCommand::Undo()
    {
        const GUID ownerGuid(m_OwnerGuidHigh, m_OwnerGuidLow);
        m_SceneEditor.ApplySetObjectProperty(ownerGuid, m_OwnerClassName, m_PropertyName, m_BeforeValue);
    }

    const char* SetObjectPropertyCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

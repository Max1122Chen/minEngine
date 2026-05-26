#include "Commands/Scene/SetObjectPropertyCommand.h"

#include "Scene/SceneEditor.h"

#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    SetObjectPropertyCommand::SetObjectPropertyCommand(SceneEditor& sceneEditor,
                                                       const GUID& ownerGuid,
                                                       std::string ownerClassName,
                                                       std::string propertyPath,
                                                       std::vector<uint8_t> beforeValue,
                                                       std::vector<uint8_t> afterValue)
        : m_SceneEditor(sceneEditor)
        , m_OwnerGuidHigh(ownerGuid.High)
        , m_OwnerGuidLow(ownerGuid.Low)
        , m_OwnerClassName(std::move(ownerClassName))
        , m_PropertyPath(std::move(propertyPath))
        , m_BeforeValue(std::move(beforeValue))
        , m_AfterValue(std::move(afterValue))
    {
        m_Description = "Set " + m_PropertyPath;
    }

    void SetObjectPropertyCommand::Execute()
    {
        const GUID ownerGuid(m_OwnerGuidHigh, m_OwnerGuidLow);
        m_SceneEditor.ApplySetObjectProperty(ownerGuid, m_OwnerClassName, m_PropertyPath, m_AfterValue);
    }

    void SetObjectPropertyCommand::Undo()
    {
        const GUID ownerGuid(m_OwnerGuidHigh, m_OwnerGuidLow);
        m_SceneEditor.ApplySetObjectProperty(ownerGuid, m_OwnerClassName, m_PropertyPath, m_BeforeValue);
    }

    const char* SetObjectPropertyCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    struct GUID;
    class SceneEditor;

    class SetObjectPropertyCommand final : public IEditorCommand
    {
    public:
        SetObjectPropertyCommand(SceneEditor& sceneEditor,
                                 const GUID& ownerGuid,
                                 std::string ownerClassName,
                                 std::string propertyName,
                                 std::vector<uint8_t> beforeValue,
                                 std::vector<uint8_t> afterValue);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_OwnerGuidHigh = 0;
        uint64_t m_OwnerGuidLow = 0;
        std::string m_OwnerClassName;
        std::string m_PropertyName;
        std::vector<uint8_t> m_BeforeValue;
        std::vector<uint8_t> m_AfterValue;
        mutable std::string m_Description;
    };
}

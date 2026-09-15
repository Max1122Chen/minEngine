#pragma once

#include "Commands/EditorSetObjectPropertyTarget.h"
#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace minEngine
{
    struct GUID;

    struct EditorSetObjectPropertySideEffects
    {
        std::function<void()> AfterExecute;
        std::function<void()> AfterUndo;
    };

    class EditorSetObjectPropertyCommand final : public EditorCommand
    {
    public:
        EditorSetObjectPropertyCommand(EditorSetObjectPropertyTarget& target,
                                       const GUID& ownerGuid,
                                       std::string ownerClassName,
                                       std::string propertyPath,
                                       std::vector<uint8_t> beforeValue,
                                       std::vector<uint8_t> afterValue,
                                       bool applyOnFirstExecute = true,
                                       EditorSetObjectPropertySideEffects sideEffects = {});

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        EditorSetObjectPropertyTarget& m_Target;
        uint64_t m_OwnerGuidHigh = 0;
        uint64_t m_OwnerGuidLow = 0;
        std::string m_OwnerClassName;
        std::string m_PropertyPath;
        std::vector<uint8_t> m_BeforeValue;
        std::vector<uint8_t> m_AfterValue;
        EditorSetObjectPropertySideEffects m_SideEffects;
        // UI may already apply the after-value; first Execute then only records for Undo/Redo.
        bool m_ApplyOnNextExecute = true;
        mutable std::string m_Description;
    };
}

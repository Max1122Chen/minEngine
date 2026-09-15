#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    class MaterialEditor;

    class EditorDisconnectMaterialPinCommand final : public EditorCommand
    {
    public:
        EditorDisconnectMaterialPinCommand(MaterialEditor& materialEditor,
                                           uint64_t toNodeDefHigh,
                                           uint64_t toNodeDefLow,
                                           int32_t toInputIndex,
                                           uint64_t fromNodeDefHigh,
                                           uint64_t fromNodeDefLow,
                                           int32_t fromOutputIndex);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        MaterialEditor& m_MaterialEditor;
        uint64_t m_ToNodeDefHigh = 0;
        uint64_t m_ToNodeDefLow = 0;
        int32_t m_ToInputIndex = 0;
        uint64_t m_FromNodeDefHigh = 0;
        uint64_t m_FromNodeDefLow = 0;
        int32_t m_FromOutputIndex = 0;
        mutable std::string m_Description;
    };
}

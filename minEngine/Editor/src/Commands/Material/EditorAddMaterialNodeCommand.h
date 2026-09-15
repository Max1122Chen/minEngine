#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    class MaterialEditor;

    class EditorAddMaterialNodeCommand final : public EditorCommand
    {
    public:
        EditorAddMaterialNodeCommand(MaterialEditor& materialEditor,
                                     std::string nodeDefClassName,
                                     float editorPosX,
                                     float editorPosY);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        MaterialEditor& m_MaterialEditor;
        std::string m_NodeDefClassName;
        float m_EditorPosX = 0.0f;
        float m_EditorPosY = 0.0f;
        uint64_t m_CreatedNodeDefHigh = 0;
        uint64_t m_CreatedNodeDefLow = 0;
        bool m_HasCreatedNode = false;
        mutable std::string m_Description;
    };
}

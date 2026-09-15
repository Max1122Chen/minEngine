#pragma once

#include "Commands/Material/MaterialTopologyTypes.h"
#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    class MaterialEditor;

    class EditorRemoveMaterialNodeCommand final : public EditorCommand
    {
    public:
        EditorRemoveMaterialNodeCommand(MaterialEditor& materialEditor,
                                        uint64_t nodeDefHigh,
                                        uint64_t nodeDefLow,
                                        std::vector<uint8_t> nodeSnapshot,
                                        std::vector<MaterialNodeInboundLink> inboundLinks);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        MaterialEditor& m_MaterialEditor;
        uint64_t m_NodeDefHigh = 0;
        uint64_t m_NodeDefLow = 0;
        std::vector<uint8_t> m_NodeSnapshot;
        std::vector<MaterialNodeInboundLink> m_InboundLinks;
        bool m_Removed = false;
        mutable std::string m_Description;
    };
}

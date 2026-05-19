#include "MIRGraph.h"

namespace minEngine
{
    MIRBlock* MIRGraph::CreateBlock(MIRBlock* parent)
    {
        auto owned = std::make_unique<MIRBlock>();
        owned->Parent = parent;
        owned->FirstInstruction = nullptr;
        MIRBlock* block = owned.get();
        m_BlockOwners.push_back(std::move(owned));
        return block;
    }
}

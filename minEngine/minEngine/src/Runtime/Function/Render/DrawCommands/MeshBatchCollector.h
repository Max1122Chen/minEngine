#pragma once

#include "Runtime/Function/Render/DrawCommands/MeshBatch.h"

#include <vector>

namespace minEngine
{
    class MeshPassProcessor;

    /** Frame-local list of mesh batches. Does not own GPU resources. */
    class MeshBatchCollector
    {
    public:
        void AddMesh(const MeshBatch& batch);
        void Clear();
        const std::vector<MeshBatch>& GetBatches() const { return m_Batches; }

        void DispatchTo(MeshPassProcessor& processor) const;

    private:
        std::vector<MeshBatch> m_Batches;
    };
}

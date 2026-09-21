#include "MeshBatchCollector.h"
#include "Runtime/Function/Render/DrawCommands/MeshPassProcessor.h"

namespace minEngine
{
    void MeshBatchCollector::AddMesh(const MeshBatch& batch)
    {
        m_Batches.push_back(batch);
    }

    void MeshBatchCollector::Clear()
    {
        m_Batches.clear();
    }

    void MeshBatchCollector::DispatchTo(MeshPassProcessor& processor) const
    {
        for (const MeshBatch& batch : m_Batches)
        {
            processor.AddMeshBatch(batch);
        }
    }
}

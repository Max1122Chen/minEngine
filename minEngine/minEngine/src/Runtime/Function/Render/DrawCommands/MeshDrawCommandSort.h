#pragma once

#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

#include <vector>

namespace minEngine
{
    /** Opaque SortKey: Material(24) | VIL(20) | VB(19) | Skinned(1). Frame-local grouping only. */
    MeshDrawCommandSortKey MakeOpaqueSortKey(const MeshDrawCommand& command);

    void FillOpaqueSortKeys(std::vector<MeshDrawCommand>& commands);

    void SortOpaqueMeshDraws(std::vector<MeshDrawCommand>& commands);

    /**
     * Far-to-near by world Bounds center distance to camera.
     * Invalid bounds fall back to LocalToWorld translation.
     */
    void SortTranslucentMeshDraws(
        std::vector<MeshDrawCommand>& commands,
        const Vector3& cameraPosition);

    float GetMeshDrawViewDistanceSq(const MeshDrawCommand& command, const Vector3& cameraPosition);
}

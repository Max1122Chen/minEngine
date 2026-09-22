#include "MeshDrawCommandSort.h"

#include "Runtime/Core/Profiling/ProfileMacros.h"

#include <algorithm>
#include <cstdint>

namespace minEngine
{
    MeshDrawCommandSortKey MakeOpaqueSortKey(const MeshDrawCommand& command)
    {
        constexpr uint32_t kMaterialBits = 24;
        constexpr uint32_t kLayoutBits = 20;
        constexpr uint32_t kVertexBits = 19;

        auto foldPointerBits = [](const void* pointer, uint32_t bitCount) -> uint64_t
        {
            if (pointer == nullptr || bitCount == 0)
            {
                return 0;
            }

            const uint64_t mask = (bitCount >= 64) ? ~0ull : ((1ull << bitCount) - 1ull);
            return (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)) >> 4) & mask;
        };

        const uint64_t materialFold = foldPointerBits(command.m_MaterialProxy, kMaterialBits);
        const uint64_t layoutFold = foldPointerBits(command.m_VertexInputLayout, kLayoutBits);
        const uint64_t vertexFold = foldPointerBits(command.m_VertexBuffer, kVertexBits);
        const uint64_t skinnedBit = command.m_bSkinned ? 1ull : 0ull;

        MeshDrawCommandSortKey sortKey;
        sortKey.m_Key =
            (materialFold << 40) | (layoutFold << 20) | (vertexFold << 1) | skinnedBit;
        return sortKey;
    }

    void FillOpaqueSortKeys(std::vector<MeshDrawCommand>& commands)
    {
        for (MeshDrawCommand& command : commands)
        {
            command.m_SortKey = MakeOpaqueSortKey(command);
        }
    }

    void SortOpaqueMeshDraws(std::vector<MeshDrawCommand>& commands)
    {
        ME_PROFILE_SCOPE("MeshDraw.SortOpaque");

        FillOpaqueSortKeys(commands);
        std::stable_sort(
            commands.begin(),
            commands.end(),
            [](const MeshDrawCommand& a, const MeshDrawCommand& b)
            {
                return a.m_SortKey < b.m_SortKey;
            });
    }

    float GetMeshDrawViewDistanceSq(const MeshDrawCommand& command, const Vector3& cameraPosition)
    {
        Vector3 center;
        if (command.m_BoundingBox.IsValid())
        {
            center = command.m_BoundingBox.GetCenter();
        }
        else
        {
            center = Vector3(command.m_ModelMatrix[3]);
        }

        const Vector3 delta = center - cameraPosition;
        return glm::dot(delta, delta);
    }

    void SortTranslucentMeshDraws(
        std::vector<MeshDrawCommand>& commands,
        const Vector3& cameraPosition)
    {
        ME_PROFILE_SCOPE("MeshDraw.SortTranslucent");

        std::stable_sort(
            commands.begin(),
            commands.end(),
            [&cameraPosition](const MeshDrawCommand& a, const MeshDrawCommand& b)
            {
                return GetMeshDrawViewDistanceSq(a, cameraPosition)
                    > GetMeshDrawViewDistanceSq(b, cameraPosition);
            });
    }
}

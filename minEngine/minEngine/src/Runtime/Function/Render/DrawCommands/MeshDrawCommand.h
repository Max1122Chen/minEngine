#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    class VertexBuffer;
    class VertexDefinition;
    class IndexBuffer;
    class Material;

    class MeshDrawCommandSortKey
    {
    public:
        MeshDrawCommandSortKey() = default;
        virtual ~MeshDrawCommandSortKey() = default;

        union 
        {
            uint64_t m_Key = 0;
        };

        bool operator < (const MeshDrawCommandSortKey& other) const
        {
            return m_Key < other.m_Key;
        }

        bool operator == (const MeshDrawCommandSortKey& other) const
        {
            return m_Key == other.m_Key;
        }

        bool operator != (const MeshDrawCommandSortKey& other) const
        {
            return m_Key != other.m_Key;
        }
    };

    class MeshDrawCommand
    {
    public:
        MeshDrawCommand() = default;
        virtual ~MeshDrawCommand() = default;

        VertexBuffer* m_VertexBuffer = nullptr;
        VertexDefinition* m_VertexDefinition = nullptr;
        IndexBuffer* m_IndexBuffer = nullptr;

        Material* m_Material = nullptr;

        Matrix4 m_ModelMatrix;

        MeshDrawCommandSortKey m_SortKey;
    };

    
}
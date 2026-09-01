#pragma once

#include "DebugDrawTypes.h"

#include <vector>

namespace minEngine
{
    class DebugDrawService
    {
    public:
        static DebugDrawService& Get();

        void ClearFrameQueues();
        void BuildFrameGeometry();

        const std::vector<DebugVertex>& GetVertices(EDebugDepthMode mode) const;
        uint32_t GetVertexCount(EDebugDepthMode mode) const;
        bool HasAnyGeometry() const;

        void EnqueueLine(DebugLineCommand command);
        void EnqueuePoint(DebugPointCommand command);
        void EnqueueBox(DebugBoxCommand command);

    private:
        std::vector<DebugLineCommand> m_Lines;
        std::vector<DebugPointCommand> m_Points;
        std::vector<DebugBoxCommand> m_Boxes;

        std::vector<DebugVertex> m_VerticesDepthTested;
        std::vector<DebugVertex> m_VerticesAlwaysVisible;
    };
}

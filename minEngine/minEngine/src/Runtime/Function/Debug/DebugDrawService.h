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

        // The command structs are several dozen bytes wide; taking them by reference keeps
        // the per-frame enqueue path free of by-value argument copies.
        void EnqueueLine(const DebugLineCommand& command);
        void EnqueuePoint(const DebugPointCommand& command);
        void EnqueueBox(const DebugBoxCommand& command);
        void EnqueueSphere(const DebugSphereCommand& command);
        void EnqueueCapsule(const DebugCapsuleCommand& command);

    private:
        std::vector<DebugLineCommand> m_Lines;
        std::vector<DebugPointCommand> m_Points;
        std::vector<DebugBoxCommand> m_Boxes;
        std::vector<DebugSphereCommand> m_Spheres;
        std::vector<DebugCapsuleCommand> m_Capsules;

        std::vector<DebugVertex> m_VerticesDepthTested;
        std::vector<DebugVertex> m_VerticesAlwaysVisible;
    };
}

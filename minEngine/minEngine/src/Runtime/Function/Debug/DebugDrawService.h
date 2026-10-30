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

        /**
         * Drawing side: drop whatever the previous frame queued and take this frame's geometry from
         * the snapshot the world side published. The commands are copied out, so the snapshot is not
         * retained and nothing outlives the frame it was published for. A null snapshot is a frame
         * with no debug geometry to draw.
         */
        void ApplyFrameSnapshot(const DebugDrawSnapshotRef& snapshot);

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
        void EnqueueIndexedLines(const DebugIndexedLinesCommand& command);

    private:
        std::vector<DebugLineCommand> m_Lines;
        std::vector<DebugPointCommand> m_Points;
        std::vector<DebugBoxCommand> m_Boxes;
        std::vector<DebugSphereCommand> m_Spheres;
        std::vector<DebugCapsuleCommand> m_Capsules;
        std::vector<DebugIndexedLinesCommand> m_IndexedLines;

        std::vector<DebugVertex> m_VerticesDepthTested;
        std::vector<DebugVertex> m_VerticesAlwaysVisible;
    };
}

#include "DebugDrawService.h"

#include "DebugGeometry.h"
#include "Runtime/Core/Log/LogSystem.h"

namespace minEngine
{
    namespace
    {
        constexpr uint32_t kSoftMaxVertices = 1'000'000u;
    }

    DebugDrawService& DebugDrawService::Get()
    {
        static DebugDrawService instance;
        return instance;
    }

    void DebugDrawService::ClearFrameQueues()
    {
        m_Lines.clear();
        m_Points.clear();
        m_Boxes.clear();
        m_Spheres.clear();
        m_Capsules.clear();
        m_IndexedLines.clear();
    }

    void DebugDrawService::ApplyFrameSnapshot(const DebugDrawSnapshotRef& snapshot)
    {
        ClearFrameQueues();
        if (snapshot == nullptr)
        {
            return;
        }

        m_Boxes.insert(m_Boxes.end(), snapshot->Boxes.begin(), snapshot->Boxes.end());
        m_Spheres.insert(m_Spheres.end(), snapshot->Spheres.begin(), snapshot->Spheres.end());
        m_Capsules.insert(m_Capsules.end(), snapshot->Capsules.begin(), snapshot->Capsules.end());
        m_IndexedLines.insert(
            m_IndexedLines.end(),
            snapshot->IndexedLines.begin(),
            snapshot->IndexedLines.end());
    }

    void DebugDrawService::BuildFrameGeometry()
    {
        m_VerticesDepthTested.clear();
        m_VerticesAlwaysVisible.clear();

        auto appendWithLimit = [](std::vector<DebugVertex>& target, const auto& appendFn)
        {
            const uint32_t before = static_cast<uint32_t>(target.size());
            appendFn(target);
            if (target.size() > kSoftMaxVertices)
            {
                ME_LOG(LogRender, Warn, "DebugDrawService: vertex count exceeded soft limit ({}); truncating.", kSoftMaxVertices);
                target.resize(kSoftMaxVertices);
            }
            (void)before;
        };

        for (const DebugLineCommand& command : m_Lines)
        {
            std::vector<DebugVertex>* vertices = command.DepthMode == EDebugDepthMode::AlwaysVisible
                ? &m_VerticesAlwaysVisible
                : &m_VerticesDepthTested;
            appendWithLimit(*vertices, [&](std::vector<DebugVertex>& out)
            {
                DebugGeometry::AppendLine(out, command.Start, command.End, command.Color);
            });
        }

        for (const DebugPointCommand& command : m_Points)
        {
            std::vector<DebugVertex>* vertices = command.DepthMode == EDebugDepthMode::AlwaysVisible
                ? &m_VerticesAlwaysVisible
                : &m_VerticesDepthTested;
            appendWithLimit(*vertices, [&](std::vector<DebugVertex>& out)
            {
                DebugGeometry::AppendPointCross(out, command.Position, command.Size, command.Color);
            });
        }

        for (const DebugBoxCommand& command : m_Boxes)
        {
            std::vector<DebugVertex>* vertices = command.DepthMode == EDebugDepthMode::AlwaysVisible
                ? &m_VerticesAlwaysVisible
                : &m_VerticesDepthTested;
            appendWithLimit(*vertices, [&](std::vector<DebugVertex>& out)
            {
                DebugGeometry::AppendBoxWireframe(out, command.WorldTransform, command.HalfExtent, command.Color);
            });
        }

        for (const DebugSphereCommand& command : m_Spheres)
        {
            std::vector<DebugVertex>* vertices = command.DepthMode == EDebugDepthMode::AlwaysVisible
                ? &m_VerticesAlwaysVisible
                : &m_VerticesDepthTested;
            appendWithLimit(*vertices, [&](std::vector<DebugVertex>& out)
            {
                DebugGeometry::AppendSphereWireframe(
                    out,
                    command.WorldTransform,
                    command.Radius,
                    command.Color,
                    command.Segments);
            });
        }

        for (const DebugCapsuleCommand& command : m_Capsules)
        {
            std::vector<DebugVertex>* vertices = command.DepthMode == EDebugDepthMode::AlwaysVisible
                ? &m_VerticesAlwaysVisible
                : &m_VerticesDepthTested;
            appendWithLimit(*vertices, [&](std::vector<DebugVertex>& out)
            {
                DebugGeometry::AppendCapsuleWireframe(
                    out,
                    command.WorldTransform,
                    command.Radius,
                    command.HalfHeight,
                    command.Color,
                    command.Segments);
            });
        }

        for (const DebugIndexedLinesCommand& command : m_IndexedLines)
        {
            if ((command.Indices.size() % 2u) != 0u)
            {
                ME_LOG(
                    LogRender,
                    Warn,
                    "DebugDrawService: IndexedLines index count {} is odd; skipping command.",
                    command.Indices.size());
                continue;
            }

            std::vector<DebugVertex>* vertices = command.DepthMode == EDebugDepthMode::AlwaysVisible
                ? &m_VerticesAlwaysVisible
                : &m_VerticesDepthTested;
            appendWithLimit(*vertices, [&](std::vector<DebugVertex>& out)
            {
                const uint32_t vertexCount = static_cast<uint32_t>(command.Vertices.size());
                for (size_t i = 0; i + 1 < command.Indices.size(); i += 2)
                {
                    const uint32_t i0 = command.Indices[i];
                    const uint32_t i1 = command.Indices[i + 1];
                    if (i0 >= vertexCount || i1 >= vertexCount)
                    {
                        ME_LOG(
                            LogRender,
                            Warn,
                            "DebugDrawService: IndexedLines index out of range ({}, {}); skipping segment.",
                            i0,
                            i1);
                        continue;
                    }

                    // LineList expansion uses the start vertex colour for the whole segment.
                    DebugGeometry::AppendLine(
                        out,
                        command.Vertices[i0].Position,
                        command.Vertices[i1].Position,
                        command.Vertices[i0].Color);
                }
            });
        }
    }

    const std::vector<DebugVertex>& DebugDrawService::GetVertices(EDebugDepthMode mode) const
    {
        if (mode == EDebugDepthMode::AlwaysVisible)
        {
            return m_VerticesAlwaysVisible;
        }
        return m_VerticesDepthTested;
    }

    uint32_t DebugDrawService::GetVertexCount(EDebugDepthMode mode) const
    {
        return static_cast<uint32_t>(GetVertices(mode).size());
    }

    bool DebugDrawService::HasAnyGeometry() const
    {
        return !m_VerticesDepthTested.empty() || !m_VerticesAlwaysVisible.empty();
    }

    void DebugDrawService::EnqueueLine(const DebugLineCommand& command)
    {
        m_Lines.push_back(command);
    }

    void DebugDrawService::EnqueuePoint(const DebugPointCommand& command)
    {
        m_Points.push_back(command);
    }

    void DebugDrawService::EnqueueBox(const DebugBoxCommand& command)
    {
        m_Boxes.push_back(command);
    }

    void DebugDrawService::EnqueueSphere(const DebugSphereCommand& command)
    {
        m_Spheres.push_back(command);
    }

    void DebugDrawService::EnqueueCapsule(const DebugCapsuleCommand& command)
    {
        m_Capsules.push_back(command);
    }

    void DebugDrawService::EnqueueIndexedLines(const DebugIndexedLinesCommand& command)
    {
        m_IndexedLines.push_back(command);
    }
}

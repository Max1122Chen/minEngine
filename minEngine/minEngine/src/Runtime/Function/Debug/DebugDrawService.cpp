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
                ME_CORE_WARN("DebugDrawService: vertex count exceeded soft limit ({}); truncating.", kSoftMaxVertices);
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

    void DebugDrawService::EnqueueLine(DebugLineCommand command)
    {
        m_Lines.push_back(command);
    }

    void DebugDrawService::EnqueuePoint(DebugPointCommand command)
    {
        m_Points.push_back(command);
    }

    void DebugDrawService::EnqueueBox(DebugBoxCommand command)
    {
        m_Boxes.push_back(command);
    }
}

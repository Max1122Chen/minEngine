#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommandSort.h"

#include "doctest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>

TEST_CASE("mesh-draw-sort: opaque sort keys are non-decreasing after stable_sort [smoke]")
{
    using namespace minEngine;

    MeshDrawCommand a;
    MeshDrawCommand b;
    MeshDrawCommand c;

    // Distinct fake pointers so folded keys differ without real GPU objects.
    a.m_MaterialProxy = reinterpret_cast<MaterialRenderProxy*>(static_cast<uintptr_t>(0x1000));
    b.m_MaterialProxy = reinterpret_cast<MaterialRenderProxy*>(static_cast<uintptr_t>(0x3000));
    c.m_MaterialProxy = reinterpret_cast<MaterialRenderProxy*>(static_cast<uintptr_t>(0x2000));
    a.m_VertexInputLayout = reinterpret_cast<RHIVertexInputLayout*>(static_cast<uintptr_t>(0x10));
    b.m_VertexInputLayout = a.m_VertexInputLayout;
    c.m_VertexInputLayout = a.m_VertexInputLayout;
    a.m_VertexBuffer = reinterpret_cast<RHIBuffer*>(static_cast<uintptr_t>(0x20));
    b.m_VertexBuffer = a.m_VertexBuffer;
    c.m_VertexBuffer = a.m_VertexBuffer;

    std::vector<MeshDrawCommand> commands{b, a, c};
    SortOpaqueMeshDraws(commands);

    CHECK(commands[0].m_SortKey.m_Key <= commands[1].m_SortKey.m_Key);
    CHECK(commands[1].m_SortKey.m_Key <= commands[2].m_SortKey.m_Key);
    CHECK(commands[0].m_MaterialProxy == a.m_MaterialProxy);
    CHECK(commands[1].m_MaterialProxy == c.m_MaterialProxy);
    CHECK(commands[2].m_MaterialProxy == b.m_MaterialProxy);
}

TEST_CASE("mesh-draw-sort: translucent far draws before near [full]")
{
    using namespace minEngine;

    MeshDrawCommand nearCmd;
    nearCmd.m_BoundingBox = Math::Geometry::AABB(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f));

    MeshDrawCommand farCmd;
    farCmd.m_BoundingBox = Math::Geometry::AABB(Vector3(9.5f, -0.5f, -0.5f), Vector3(10.5f, 0.5f, 0.5f));

    std::vector<MeshDrawCommand> commands{nearCmd, farCmd};
    const Vector3 cameraPos(0.0f, 0.0f, 5.0f);
    SortTranslucentMeshDraws(commands, cameraPos);

    CHECK(GetMeshDrawViewDistanceSq(commands[0], cameraPos)
        >= GetMeshDrawViewDistanceSq(commands[1], cameraPos));
    CHECK(commands[0].m_BoundingBox.GetCenter().x == doctest::Approx(10.0f));
    CHECK(commands[1].m_BoundingBox.GetCenter().x == doctest::Approx(0.0f));
}

TEST_CASE("mesh-draw-sort: translucent falls back to model translation when bounds invalid [full]")
{
    using namespace minEngine;

    MeshDrawCommand nearCmd;
    nearCmd.m_ModelMatrix = glm::translate(Matrix4(1.0f), Vector3(0.0f, 0.0f, 0.0f));

    MeshDrawCommand farCmd;
    farCmd.m_ModelMatrix = glm::translate(Matrix4(1.0f), Vector3(0.0f, 0.0f, -20.0f));

    std::vector<MeshDrawCommand> commands{nearCmd, farCmd};
    const Vector3 cameraPos(0.0f, 0.0f, 5.0f);
    SortTranslucentMeshDraws(commands, cameraPos);

    CHECK(GetMeshDrawViewDistanceSq(commands[0], cameraPos)
        > GetMeshDrawViewDistanceSq(commands[1], cameraPos));
}

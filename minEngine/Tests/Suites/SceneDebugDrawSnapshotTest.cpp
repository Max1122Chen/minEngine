#include "SceneDebugDrawSnapshotTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "EngineTestFixture.h"

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Debug/DebugDrawService.h"
#include "Runtime/Function/Debug/DebugDrawTypes.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/BoxColliderComponent.h"
#include "Runtime/Function/Physics/PhysicsDebugDraw.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"

#include "doctest.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace
{
    using minEngine::DebugDrawService;
    using minEngine::DebugDrawSnapshotRef;
    using minEngine::DebugVertex;
    using minEngine::EDebugDepthMode;
    using minEngine::PhysicsDebugDraw::AccessProbe;
    using minEngine::PhysicsDebugDraw::ProbeGameObjectAccess;
    using minEngine::RenderCommandPipe;
    using minEngine::Scene;
    using minEngine::SceneDrawDesc;

    /** Registries a Scene destructor and the command pipe touch. */
    class DebugDrawSnapshotScope
    {
    public:
        DebugDrawSnapshotScope()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            minEngine::ObjectManager::Initialize();

            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            m_Pipe.Initialize();
            DebugDrawService::Get().ClearFrameQueues();
            DebugDrawService::Get().BuildFrameGeometry();
        }

        ~DebugDrawSnapshotScope()
        {
            DebugDrawService::Get().ClearFrameQueues();
            DebugDrawService::Get().BuildFrameGeometry();
            m_Pipe.Shutdown();
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(nullptr);
            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

        DebugDrawSnapshotScope(const DebugDrawSnapshotScope&) = delete;
        DebugDrawSnapshotScope& operator=(const DebugDrawSnapshotScope&) = delete;

        RenderCommandPipe& Pipe() { return m_Pipe; }

    private:
        minEngine::ObjectManager m_ObjectManager;
        minEngine::SceneManager m_SceneManager;
        RenderCommandPipe m_Pipe;
    };

    /**
     * One box collider on one game object. Its half extent and channel are the two things the
     * snapshot has to carry: the extent is the geometry, the channel is the colour.
     */
    const minEngine::Math::Vector3 kHalfExtent(1.0f, 2.0f, 3.0f);
    const minEngine::Math::Vector3 kObjectPosition(5.0f, 0.0f, 0.0f);

    std::shared_ptr<minEngine::GameObject> AddBoxColliderObject(const std::shared_ptr<Scene>& scene)
    {
        const std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
        if (!gameObject)
        {
            return nullptr;
        }

        const std::shared_ptr<minEngine::SceneComponent> root =
            gameObject->AddComponent<minEngine::SceneComponent>();
        if (!root)
        {
            return nullptr;
        }
        root->SetPosition(kObjectPosition);

        const std::shared_ptr<minEngine::BoxColliderComponent> collider =
            gameObject->AddComponent<minEngine::BoxColliderComponent>();
        if (!collider)
        {
            return nullptr;
        }
        collider->SetObjectChannel(minEngine::ECollisionChannel::WorldStatic);
        collider->SetHalfExtent(kHalfExtent);
        return gameObject;
    }

    /** Reads the counters an interval moved, so a test can state "this step read nothing". */
    AccessProbe ProbeDelta(const AccessProbe& before, const AccessProbe& after)
    {
        AccessProbe delta;
        delta.SceneWalks = after.SceneWalks - before.SceneWalks;
        delta.GameObjectReads = after.GameObjectReads - before.GameObjectReads;
        delta.ColliderReads = after.ColliderReads - before.ColliderReads;
        return delta;
    }

    void PrintGeometry(const char* label, uint32_t vertexCount)
    {
        std::printf("[scene-debug-draw] %s: vertices=%u\n", label, vertexCount);
    }
}

TEST_CASE("scene-debug-draw-snapshot: the world side publishes collider geometry as plain values [smoke]")
{
    minEngine::EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    DebugDrawSnapshotScope scope;

    const std::shared_ptr<Scene> scene = minEngine::NewObject<Scene>("debug-draw-snapshot");
    REQUIRE(scene != nullptr);
    REQUIRE(AddBoxColliderObject(scene) != nullptr);

    // The probe is live: building the snapshot is a read of the world and has to be counted, or a
    // zero on the drawing side would only mean "nobody counted".
    const AccessProbe before = ProbeGameObjectAccess();
    const DebugDrawSnapshotRef snapshot = minEngine::PhysicsDebugDraw::BuildSnapshot(*scene);
    const AccessProbe fromWorldSide = ProbeDelta(before, ProbeGameObjectAccess());

    std::printf(
        "[scene-debug-draw] world side reads: walks=%llu objects=%llu colliders=%llu\n",
        static_cast<unsigned long long>(fromWorldSide.SceneWalks),
        static_cast<unsigned long long>(fromWorldSide.GameObjectReads),
        static_cast<unsigned long long>(fromWorldSide.ColliderReads));

    CHECK(fromWorldSide.SceneWalks == 1u);
    CHECK(fromWorldSide.GameObjectReads == 1u);
    CHECK(fromWorldSide.ColliderReads == 1u);

    REQUIRE(snapshot != nullptr);
    REQUIRE(snapshot->Boxes.size() == 1u);
    CHECK(snapshot->Spheres.empty());
    CHECK(snapshot->Capsules.empty());

    const minEngine::DebugBoxCommand& box = snapshot->Boxes[0];
    CHECK(box.HalfExtent.x == doctest::Approx(kHalfExtent.x));
    CHECK(box.HalfExtent.y == doctest::Approx(kHalfExtent.y));
    CHECK(box.HalfExtent.z == doctest::Approx(kHalfExtent.z));
    // WorldStatic keeps the colour it has always had on screen.
    CHECK(box.Color.x == doctest::Approx(0.4f));
    CHECK(box.Color.y == doctest::Approx(0.8f));
    CHECK(box.Color.z == doctest::Approx(0.4f));
    CHECK(box.Color.w == doctest::Approx(1.0f));
    // The pose is read through the root component, so it is the object's world position.
    CHECK(box.WorldTransform[3][0] == doctest::Approx(kObjectPosition.x));
    CHECK(box.WorldTransform[3][1] == doctest::Approx(kObjectPosition.y));
    CHECK(box.WorldTransform[3][2] == doctest::Approx(kObjectPosition.z));
}

TEST_CASE("scene-debug-draw-snapshot: drawing a frame reads no game object [smoke]")
{
    minEngine::EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    DebugDrawSnapshotScope scope;

    const std::shared_ptr<Scene> scene = minEngine::NewObject<Scene>("debug-draw-frame");
    REQUIRE(scene != nullptr);
    REQUIRE(AddBoxColliderObject(scene) != nullptr);

    // World side: one frame's geometry, published into the view intent.
    const DebugDrawSnapshotRef snapshot = minEngine::PhysicsDebugDraw::BuildSnapshot(*scene);
    REQUIRE(snapshot != nullptr);
    CHECK(snapshot.use_count() == 1u);

    SceneDrawDesc desc;
    desc.Flags = minEngine::SceneDrawFlags::EnableDebugDraw;
    desc.DebugSnapshot = snapshot;
    CHECK(snapshot.use_count() == 2u);

    scope.Pipe().SubmitViewIntent(desc);
    CHECK(snapshot.use_count() == 3u);

    std::vector<SceneDrawDesc> intents;
    scope.Pipe().TakeViewIntents(0, intents);
    REQUIRE(intents.size() == 1u);
    REQUIRE(intents[0].DebugSnapshot != nullptr);

    // Drawing side: the probe window covers exactly the step a render frame takes, from the intent
    // it was handed to the vertices it will submit.
    DebugDrawService& service = DebugDrawService::Get();
    const AccessProbe before = ProbeGameObjectAccess();
    service.ApplyFrameSnapshot(intents[0].DebugSnapshot);
    service.BuildFrameGeometry();
    const AccessProbe drawingSide = ProbeDelta(before, ProbeGameObjectAccess());

    const uint32_t vertexCount = service.GetVertexCount(EDebugDepthMode::Tested);
    PrintGeometry("frame", vertexCount);
    std::printf(
        "[scene-debug-draw] drawing side reads: walks=%llu objects=%llu colliders=%llu\n",
        static_cast<unsigned long long>(drawingSide.SceneWalks),
        static_cast<unsigned long long>(drawingSide.GameObjectReads),
        static_cast<unsigned long long>(drawingSide.ColliderReads));

    CHECK(service.HasAnyGeometry());
    // A box wireframe is twelve edges of two vertices, whatever carried it here.
    CHECK(vertexCount == 24u);

    const std::vector<DebugVertex>& vertices = service.GetVertices(EDebugDepthMode::Tested);
    REQUIRE(vertices.size() == 24u);
    // First edge: local corner (-1,-2,-3) to (1,-2,-3), moved to the object's world position.
    CHECK(vertices[0].Position.x == doctest::Approx(kObjectPosition.x - kHalfExtent.x));
    CHECK(vertices[0].Position.y == doctest::Approx(-kHalfExtent.y));
    CHECK(vertices[0].Position.z == doctest::Approx(-kHalfExtent.z));
    CHECK(vertices[1].Position.x == doctest::Approx(kObjectPosition.x + kHalfExtent.x));
    CHECK(vertices[1].Position.y == doctest::Approx(-kHalfExtent.y));
    CHECK(vertices[0].Color.x == doctest::Approx(0.4f));
    CHECK(vertices[0].Color.y == doctest::Approx(0.8f));

    CHECK(drawingSide.SceneWalks == 0u);
    CHECK(drawingSide.GameObjectReads == 0u);
    CHECK(drawingSide.ColliderReads == 0u);
    CHECK(drawingSide.Total() == 0u);

    // The drawn geometry came out of values only: the service copied the commands and kept no
    // reference, so releasing the intent is the whole of this frame's lifetime.
    CHECK(snapshot.use_count() == 3u);
    intents.clear();
    CHECK(snapshot.use_count() == 2u);

    // The next frame publishes nothing, and nothing of this one is drawn again.
    service.ApplyFrameSnapshot(nullptr);
    service.BuildFrameGeometry();
    PrintGeometry("next frame", service.GetVertexCount(EDebugDepthMode::Tested));
    CHECK(service.GetVertexCount(EDebugDepthMode::Tested) == 0u);
    CHECK_FALSE(service.HasAnyGeometry());
}

TEST_CASE("scene-debug-draw-snapshot: the published geometry outlives the scene it came from [smoke]")
{
    minEngine::EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    DebugDrawSnapshotScope scope;

    DebugDrawSnapshotRef snapshot;
    {
        std::shared_ptr<Scene> scene = minEngine::NewObject<Scene>("debug-draw-outlives");
        REQUIRE(scene != nullptr);
        const std::weak_ptr<Scene> sceneWeak = scene;
        REQUIRE(AddBoxColliderObject(scene) != nullptr);

        snapshot = minEngine::PhysicsDebugDraw::BuildSnapshot(*scene);
        REQUIRE(snapshot != nullptr);

        scene.reset();
        REQUIRE(sceneWeak.expired());
    }

    // The world this geometry described is gone. Drawing it is still well defined, which is the
    // point of publishing values instead of a handle to the world.
    DebugDrawService& service = DebugDrawService::Get();
    const AccessProbe before = ProbeGameObjectAccess();
    service.ApplyFrameSnapshot(snapshot);
    service.BuildFrameGeometry();
    const AccessProbe drawingSide = ProbeDelta(before, ProbeGameObjectAccess());

    const std::vector<DebugVertex>& vertices = service.GetVertices(EDebugDepthMode::Tested);
    PrintGeometry("scene released", static_cast<uint32_t>(vertices.size()));
    REQUIRE(vertices.size() == 24u);
    CHECK(vertices[0].Position.x == doctest::Approx(kObjectPosition.x - kHalfExtent.x));
    CHECK(vertices[1].Position.x == doctest::Approx(kObjectPosition.x + kHalfExtent.x));
    CHECK(vertices[1].Color.z == doctest::Approx(0.4f));
    CHECK(drawingSide.Total() == 0u);

    service.ClearFrameQueues();
    service.BuildFrameGeometry();
}

TEST_CASE("scene-debug-draw-snapshot: IndexedLines expands to tested LineList [smoke]")
{
    using minEngine::Math::Vector3;
    using minEngine::Math::Vector4;

    DebugDrawService& service = DebugDrawService::Get();
    service.ClearFrameQueues();

    minEngine::DebugIndexedLinesCommand command;
    command.Vertices = {
        {Vector3(0.0f, 0.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)},
        {Vector3(1.0f, 0.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)},
        {Vector3(1.0f, 1.0f, 0.0f), Vector4(0.0f, 1.0f, 0.0f, 1.0f)},
        {Vector3(0.0f, 1.0f, 0.0f), Vector4(0.0f, 1.0f, 0.0f, 1.0f)},
    };
    command.Indices = {0, 1, 1, 2, 2, 3, 3, 0};
    command.DepthMode = EDebugDepthMode::Tested;
    service.EnqueueIndexedLines(command);
    service.BuildFrameGeometry();

    CHECK(service.GetVertexCount(EDebugDepthMode::Tested) == 8u);
    CHECK(service.GetVertexCount(EDebugDepthMode::AlwaysVisible) == 0u);
    const std::vector<DebugVertex>& tested = service.GetVertices(EDebugDepthMode::Tested);
    REQUIRE(tested.size() == 8u);
    CHECK(tested[0].Position.x == doctest::Approx(0.0f));
    CHECK(tested[1].Position.x == doctest::Approx(1.0f));
    CHECK(tested[0].Color.x == doctest::Approx(1.0f));

    service.ClearFrameQueues();
    service.BuildFrameGeometry();
}

TEST_CASE("scene-debug-draw-snapshot: IndexedLines AlwaysVisible lands in the visible bucket [smoke]")
{
    using minEngine::Math::Vector3;
    using minEngine::Math::Vector4;

    DebugDrawService& service = DebugDrawService::Get();
    service.ClearFrameQueues();

    minEngine::DebugIndexedLinesCommand command;
    command.Vertices = {
        {Vector3(0.0f, 0.0f, 0.0f), Vector4(0.2f, 0.4f, 0.6f, 1.0f)},
        {Vector3(0.0f, 2.0f, 0.0f), Vector4(0.2f, 0.4f, 0.6f, 1.0f)},
    };
    command.Indices = {0, 1};
    command.DepthMode = EDebugDepthMode::AlwaysVisible;
    service.EnqueueIndexedLines(command);
    service.BuildFrameGeometry();

    CHECK(service.GetVertexCount(EDebugDepthMode::Tested) == 0u);
    CHECK(service.GetVertexCount(EDebugDepthMode::AlwaysVisible) == 2u);
    const std::vector<DebugVertex>& always = service.GetVertices(EDebugDepthMode::AlwaysVisible);
    REQUIRE(always.size() == 2u);
    CHECK(always[1].Position.y == doctest::Approx(2.0f));
    CHECK(always[0].Color.z == doctest::Approx(0.6f));

    service.ClearFrameQueues();
    service.BuildFrameGeometry();
}

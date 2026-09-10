#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Function/Animation/AnimationGraph.h"
#include "Runtime/Function/Animation/AnimationGraphInstance.h"
#include "Runtime/Function/Animation/AnimationTrack.h"
#include "Runtime/Function/Animation/Pose.h"
#include "Runtime/Function/Animation/Skeleton.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Math/Quaternion.h"
#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"

#include "doctest.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{
    std::vector<uint8_t> MakeFloatDefault(float value)
    {
        std::vector<uint8_t> bytes(sizeof(float));
        std::memcpy(bytes.data(), &value, sizeof(float));
        return bytes;
    }

    std::vector<uint8_t> MakeBoolDefault(bool value)
    {
        return {static_cast<uint8_t>(value ? 1u : 0u)};
    }

    std::shared_ptr<minEngine::Skeleton> MakeSharedTwoBoneSkeleton()
    {
        using namespace minEngine;

        std::vector<SkeletonBone> bones(2);
        bones[0].Name = "Root";
        bones[0].ParentIndex = -1;
        bones[0].LocalBind = Transform{};
        bones[0].InverseBindPose = Matrix4(1.0f);

        bones[1].Name = "Child";
        bones[1].ParentIndex = 0;
        bones[1].LocalBind.Position = Vector3(0.0f, 1.0f, 0.0f);
        bones[1].InverseBindPose = Matrix4(1.0f);

        auto skeleton = std::make_shared<Skeleton>();
        std::string error;
        REQUIRE(skeleton->SetBones(std::move(bones), &error));
        return skeleton;
    }

    std::shared_ptr<minEngine::AnimationClip> MakeConstantPoseClip(
        const std::shared_ptr<minEngine::Skeleton>& skeleton,
        float childY,
        float duration = 1.0f)
    {
        using namespace minEngine;

        auto clip = std::make_shared<AnimationClip>();
        clip->SetSkeleton(skeleton);
        clip->SetDuration(duration);

        AnimationTrack track;
        track.BoneIndex = 1;
        track.PositionKeys.push_back(AnimationVec3Key{0.0f, Vector3(0.0f, childY, 0.0f)});
        track.PositionKeys.push_back(AnimationVec3Key{duration, Vector3(0.0f, childY, 0.0f)});
        clip->SetTracks({track});
        return clip;
    }
}

TEST_CASE("animation-graph: pose blend endpoints and midpoint [full]")
{
    using namespace minEngine;

    Pose a;
    a.ResetToIdentity(2);
    a.At(1).Position = Vector3(0.0f, 0.0f, 0.0f);
    a.At(1).Scale = Vector3(1.0f, 1.0f, 1.0f);

    Pose b;
    b.ResetToIdentity(2);
    b.At(1).Position = Vector3(0.0f, 10.0f, 0.0f);
    b.At(1).Scale = Vector3(3.0f, 3.0f, 3.0f);

    Pose out;
    REQUIRE(Pose::Blend(a, b, 0.0f, out));
    CHECK(out.At(1).Position.y == doctest::Approx(0.0f));

    REQUIRE(Pose::Blend(a, b, 1.0f, out));
    CHECK(out.At(1).Position.y == doctest::Approx(10.0f));
    CHECK(out.At(1).Scale.x == doctest::Approx(3.0f));

    REQUIRE(Pose::Blend(a, b, 0.5f, out));
    CHECK(out.At(1).Position.y == doctest::Approx(5.0f));
    CHECK(out.At(1).Scale.x == doctest::Approx(2.0f));

    Pose mismatch;
    mismatch.ResetToIdentity(1);
    CHECK_FALSE(Pose::Blend(a, mismatch, 0.5f, out));
}

TEST_CASE("animation-graph: idle to walk on speed [full]")
{
    using namespace minEngine;

    auto skeleton = MakeSharedTwoBoneSkeleton();
    auto idleClip = MakeConstantPoseClip(skeleton, 1.0f);
    auto walkClip = MakeConstantPoseClip(skeleton, 5.0f);

    auto graph = std::make_shared<AnimationGraph>();
    ParameterSchema schema;
    REQUIRE(schema.AddEntry({"Speed", ParameterValueType::Float, MakeFloatDefault(0.0f)}));
    graph->SetSchema(std::move(schema));

    AnimStateMachine sm;
    AnimState idle;
    idle.Name = "Idle";
    idle.Clip = idleClip;
    AnimState walk;
    walk.Name = "Walk";
    walk.Clip = walkClip;
    sm.States = {idle, walk};
    sm.DefaultStateName = "Idle";

    AnimTransition toWalk;
    toWalk.FromStateName = "Idle";
    toWalk.ToStateName = "Walk";
    toWalk.BlendDurationSeconds = 0.2f;
    AnimCondition speedHigh;
    speedHigh.ParamName = "Speed";
    speedHigh.Op = AnimConditionOp::Greater;
    speedHigh.OperandFloat = 0.1f;
    toWalk.Conditions = {speedHigh};

    AnimTransition toIdle;
    toIdle.FromStateName = "Walk";
    toIdle.ToStateName = "Idle";
    toIdle.BlendDurationSeconds = 0.2f;
    AnimCondition speedLow;
    speedLow.ParamName = "Speed";
    speedLow.Op = AnimConditionOp::LessEqual;
    speedLow.OperandFloat = 0.1f;
    toIdle.Conditions = {speedLow};

    sm.Transitions = {toWalk, toIdle};
    graph->SetStateMachine(std::move(sm));

    std::string error;
    REQUIRE(graph->Validate(&error));

    AnimationGraphInstance instance;
    instance.SetGraph(graph);
    REQUIRE(instance.IsBound());
    CHECK(instance.GetCurrentStateName() == "Idle");

    Pose pose;
    instance.Update(0.016f, pose);
    CHECK(instance.GetCurrentStateName() == "Idle");
    CHECK(pose.At(1).Position.y == doctest::Approx(1.0f));

    REQUIRE(instance.SetFloat("Speed", 1.0f));
    instance.Update(0.016f, pose);
    CHECK(instance.IsTransitioning());

    // Finish blend
    instance.Update(0.25f, pose);
    CHECK_FALSE(instance.IsTransitioning());
    CHECK(instance.GetCurrentStateName() == "Walk");
    CHECK(pose.At(1).Position.y == doctest::Approx(5.0f));
}

TEST_CASE("animation-graph: trigger any-state consume [full]")
{
    using namespace minEngine;

    auto skeleton = MakeSharedTwoBoneSkeleton();
    auto idleClip = MakeConstantPoseClip(skeleton, 1.0f);
    auto attackClip = MakeConstantPoseClip(skeleton, 9.0f);

    auto graph = std::make_shared<AnimationGraph>();
    ParameterSchema schema;
    REQUIRE(schema.AddEntry({"Attack", ParameterValueType::Bool, MakeBoolDefault(false)}));
    graph->SetSchema(std::move(schema));

    AnimStateMachine sm;
    AnimState idle;
    idle.Name = "Idle";
    idle.Clip = idleClip;
    AnimState attack;
    attack.Name = "Attack";
    attack.Clip = attackClip;
    attack.bLoop = false;
    sm.States = {idle, attack};
    sm.DefaultStateName = "Idle";

    AnimTransition anyToAttack;
    anyToAttack.ToStateName = "Attack";
    anyToAttack.BlendDurationSeconds = 0.0f;
    AnimCondition attackTrig;
    attackTrig.ParamName = "Attack";
    attackTrig.Op = AnimConditionOp::IsSet;
    anyToAttack.Conditions = {attackTrig};
    sm.AnyStateTransitions = {anyToAttack};
    graph->SetStateMachine(std::move(sm));

    REQUIRE(graph->Validate());

    AnimationGraphInstance instance;
    instance.SetGraph(graph);
    REQUIRE(instance.SetTrigger("Attack"));

    Pose pose;
    instance.Update(0.016f, pose);
    CHECK(instance.GetCurrentStateName() == "Attack");

    bool stillSet = true;
    REQUIRE(instance.GetStore().TryGetBoolByName("Attack", stillSet));
    CHECK_FALSE(stillSet);
}

TEST_CASE("animation-graph: empty clip state allowed and holds last pose [full]")
{
    using namespace minEngine;

    auto skeleton = MakeSharedTwoBoneSkeleton();
    auto idleClip = MakeConstantPoseClip(skeleton, 1.0f);

    auto graph = std::make_shared<AnimationGraph>();
    ParameterSchema schema;
    REQUIRE(schema.AddEntry({"GoEmpty", ParameterValueType::Bool, MakeBoolDefault(false)}));
    graph->SetSchema(std::move(schema));

    AnimStateMachine sm;
    AnimState idle;
    idle.Name = "Idle";
    idle.Clip = idleClip;
    AnimState empty;
    empty.Name = "Empty";
    empty.Clip = nullptr;
    sm.States = {idle, empty};
    sm.DefaultStateName = "Idle";

    AnimTransition toEmpty;
    toEmpty.FromStateName = "Idle";
    toEmpty.ToStateName = "Empty";
    toEmpty.BlendDurationSeconds = 0.0f;
    AnimCondition go;
    go.ParamName = "GoEmpty";
    go.Op = AnimConditionOp::IsSet;
    toEmpty.Conditions = {go};
    sm.Transitions = {toEmpty};
    graph->SetStateMachine(std::move(sm));

    std::string error;
    REQUIRE(graph->Validate(&error));

    AnimationGraphInstance instance;
    instance.SetGraph(graph);
    REQUIRE(instance.IsBound());

    Pose pose;
    instance.Update(0.016f, pose);
    CHECK(instance.GetCurrentStateName() == "Idle");
    CHECK(pose.At(1).Position.y == doctest::Approx(1.0f));

    REQUIRE(instance.SetTrigger("GoEmpty"));
    instance.Update(0.016f, pose);
    CHECK(instance.GetCurrentStateName() == "Empty");
    CHECK(pose.At(1).Position.y == doctest::Approx(1.0f));
}

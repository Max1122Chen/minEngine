#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Function/Animation/AnimationPlayer.h"
#include "Runtime/Function/Animation/AnimationTrack.h"
#include "Runtime/Function/Animation/Pose.h"
#include "Runtime/Function/Animation/Skeleton.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Math/Quaternion.h"

#include "doctest.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace
{
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
}

TEST_CASE("animation-clip: evaluate mid-key position [full]")
{
    using namespace minEngine;

    auto skeleton = MakeSharedTwoBoneSkeleton();
    AnimationClip clip;
    clip.SetSkeleton(skeleton);
    clip.SetDuration(1.0f);

    AnimationTrack track;
    track.BoneIndex = 1;
    track.PositionKeys.push_back(AnimationVec3Key{0.0f, Vector3(0.0f, 1.0f, 0.0f)});
    track.PositionKeys.push_back(AnimationVec3Key{1.0f, Vector3(0.0f, 3.0f, 0.0f)});
    clip.SetTracks({track});

    Pose pose;
    clip.Evaluate(0.5f, pose);
    REQUIRE(pose.GetBoneCount() == 2);
    CHECK(pose.At(1).Position.y == doctest::Approx(2.0f));
    CHECK(pose.At(0).Position.y == doctest::Approx(0.0f));
}

TEST_CASE("animation-clip: missing track component keeps bind [full]")
{
    using namespace minEngine;

    auto skeleton = MakeSharedTwoBoneSkeleton();
    AnimationClip clip;
    clip.SetSkeleton(skeleton);
    clip.SetDuration(1.0f);

    AnimationTrack track;
    track.BoneIndex = 1;
    track.PositionKeys.push_back(AnimationVec3Key{0.0f, Vector3(0.0f, 5.0f, 0.0f)});
    clip.SetTracks({track});

    Pose pose;
    clip.Evaluate(0.0f, pose);
    CHECK(pose.At(1).Position.y == doctest::Approx(5.0f));
    CHECK(pose.At(1).Scale.x == doctest::Approx(1.0f));
}

TEST_CASE("animation-clip: TryGetNamedFloat miss and hit [full]")
{
    using namespace minEngine;

    AnimationClip clip;
    float value = -1.0f;
    CHECK_FALSE(clip.TryGetNamedFloat("Missing", 0.0f, value));

    AnimationNamedFloatTrack named;
    named.Name = "Blend";
    named.Keys.push_back(AnimationFloatKey{0.0f, 0.0f});
    named.Keys.push_back(AnimationFloatKey{1.0f, 2.0f});
    clip.SetNamedFloatTracks({named});

    REQUIRE(clip.TryGetNamedFloat("Blend", 0.5f, value));
    CHECK(value == doctest::Approx(1.0f));
}

TEST_CASE("animation-clip: player loop wrap and pause at end [full]")
{
    using namespace minEngine;

    auto skeleton = MakeSharedTwoBoneSkeleton();
    auto clip = std::make_shared<AnimationClip>();
    clip->SetSkeleton(skeleton);
    clip->SetDuration(1.0f);

    AnimationTrack track;
    track.BoneIndex = 0;
    track.PositionKeys.push_back(AnimationVec3Key{0.0f, Vector3(0.0f, 0.0f, 0.0f)});
    track.PositionKeys.push_back(AnimationVec3Key{1.0f, Vector3(1.0f, 0.0f, 0.0f)});
    clip->SetTracks({track});

    AnimationPlayer player;
    player.SetClip(clip);
    player.SetLooping(true);
    player.Play();

    Pose pose;
    player.Update(1.5f, pose);
    CHECK(player.GetState() == AnimationPlayState::Playing);
    CHECK(player.GetTime() == doctest::Approx(0.5f));

    player.SetLooping(false);
    player.SetTime(0.0f);
    player.Play();
    player.Update(2.0f, pose);
    CHECK(player.GetState() == AnimationPlayState::Paused);
    CHECK(player.GetTime() == doctest::Approx(1.0f));
}
